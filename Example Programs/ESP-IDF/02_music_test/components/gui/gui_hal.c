/**
 * @file gui_hal.c
 * @brief GUI Hardware Abstraction Layer (HAL) for Audio Playback Control
 * @details This module implements the HAL layer between GUI and audio playback subsystems,
 *          handling music file scanning (MP3 only), track playback control, volume/mute management,
 *          and audio player callback handling for ESP32-based audio devices.
 * @note Depends on: file_iterator.h (file system traversal), audio_player.h (audio decoding/playback),
 *       gui_hal.h (public API declarations), board_speaker.h (speaker hardware control)
 * @version 1.0
 * @date 2025
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "file_iterator.h"
#include "audio_player.h"
#include "gui_hal.h"
#include "esp_log.h"
#include "board_speaker.h"



#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_SIMD         
#define DR_MP3_ONLY_MP3                
#define DR_MP3_FLOAT_OUTPUT      
#include "dr_mp3.h"




/* Static log tag for ESP_LOGx macros */
static const char *TAG = "GUI_HAL";

/* -------------------------------------------------------------------------- */
/* Module Configuration Constants (Internal Limits)                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Maximum length for file path strings (including null terminator)
 * @note Ensures buffer safety when handling file system paths
 */
#define GUI_HAL_MAX_PATH_LEN        256

/**
 * @brief Length of the track end event queue (number of uint32_t elements)
 * @note Controls how many auto-next track events can be queued before blocking
 */
#define GUI_HAL_TRACK_END_QUEUE_LEN 4

/* -------------------------------------------------------------------------- */
/* Audio Click/Pop Suppression (Internal)                                     */
/* -------------------------------------------------------------------------- */
/* Fade-in/out time for soft mute transitions (ms). */
#define GUI_HAL_FADE_MS            20U
/* How long to push explicit zeros into I2S DMA when PAUSE occurs (ms). */
#define GUI_HAL_PAUSE_FLUSH_MS     60U
/* Lead-in silence before ramp-up when starting a new track (ms). */
#define GUI_HAL_START_SILENCE_MS   12U


/* -------------------------------------------------------------------------- */
/* Module-Level State Variables (Private)                                    */
/* -------------------------------------------------------------------------- */

/** @brief File iterator instance for traversing music directory (/sdcard/music) */
static file_iterator_instance_t *s_file_iterator      = NULL;

/** @brief Configuration struct for audio player initialization */
static audio_player_config_t     s_player_config      = {0};

/** @brief Count of scanned music files (clipped to MAX_MP3_NUM) */
static size_t                    s_track_count        = 0U;

/** @brief Software volume/mute state: 0–100 linear volume and a separate mute flag. */
static uint8_t                   s_soft_volume        = 100U;
static bool                      s_soft_mute          = false;

/** @brief FreeRTOS queue for passing track end/auto-next events to GUI task */
static QueueHandle_t             s_track_end_queue    = NULL;

/* Runtime audio format (updated by clk_set callback). Defaults are safe fallbacks. */
static uint32_t                   s_pcm_rate_hz         = 44100U;
static uint8_t                    s_pcm_bits            = 16U;
static uint8_t                    s_pcm_channels        = 2U;

/* Smooth gain ramp (Q15) used to avoid click/pop on abrupt transitions. */
static int32_t                    s_gain_q15            = 32768; /* 1.0 */
static int32_t                    s_gain_target_q15     = 32768; /* 1.0 */

/* Force output silence for N PCM frames (handled in write_cb). */
static uint32_t                   s_force_silence_frames = 0U;


/* -------------------------------------------------------------------------- */
/* Internal Helper Functions (Private)                                       */
/* -------------------------------------------------------------------------- */

#define  BUF_SZ  8192 

uint8_t mp3_buf[BUF_SZ];

/* -------------------------------------------------------------------------- */
/* ID3 tag parsing (Artist/Album)                                             */
/* - ID3v2.3/v2.4: TPE1 (Artist), TALB (Album)                                */
/* - Fallback ID3v1: TAG (artist/album)                                       */
/* -------------------------------------------------------------------------- */

#define GUI_HAL_ID3_MAX_TEXT   96U   /* max chars stored for artist/album */

static uint32_t s_be32(const uint8_t b[4])
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static uint32_t s_synchsafe32(const uint8_t b[4])
{
    return ((uint32_t)(b[0] & 0x7Fu) << 21) | ((uint32_t)(b[1] & 0x7Fu) << 14) |
           ((uint32_t)(b[2] & 0x7Fu) << 7)  |  (uint32_t)(b[3] & 0x7Fu);
}

static void s_trim_inplace(char *s)
{
    if (!s) return;

    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t n = strlen(s);
    while (n > 0) {
        unsigned char c = (unsigned char)s[n - 1];
        if (c == '\0' || isspace(c)) {
            s[n - 1] = '\0';
            n--;
        } else {
            break;
        }
    }
}

static size_t s_utf8_write_cp(char *out, size_t out_len, size_t o, uint32_t cp)
{
    if (!out || out_len == 0) return 0;

    if (cp <= 0x7F) {
        if (o + 1 >= out_len) return o;
        out[o++] = (char)cp;
    }
    else if (cp <= 0x7FF) {
        if (o + 2 >= out_len) return o;
        out[o++] = (char)(0xC0 | ((cp >> 6) & 0x1F));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    }
    else if (cp <= 0xFFFF) {
        if (o + 3 >= out_len) return o;
        out[o++] = (char)(0xE0 | ((cp >> 12) & 0x0F));
        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    }
    else {
        if (o + 4 >= out_len) return o;
        out[o++] = (char)(0xF0 | ((cp >> 18) & 0x07));
        out[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[o++] = (char)(0x80 | (cp & 0x3F));
    }

    out[o] = '\0';
    return o;
}

static void s_utf16_to_utf8(const uint8_t *in, size_t in_len, bool be, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!in || in_len < 2) return;

    size_t o = 0;
    for (size_t i = 0; i + 1 < in_len; i += 2) {
        uint16_t w = be ? ((uint16_t)in[i] << 8) | (uint16_t)in[i + 1]
                        : ((uint16_t)in[i + 1] << 8) | (uint16_t)in[i];
        if (w == 0x0000) break;

        uint32_t cp = (uint32_t)w;

        if (w >= 0xD800 && w <= 0xDBFF && (i + 3) < in_len) {
            uint16_t w2 = be ? ((uint16_t)in[i + 2] << 8) | (uint16_t)in[i + 3]
                             : ((uint16_t)in[i + 3] << 8) | (uint16_t)in[i + 2];
            if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                cp = 0x10000u + (((uint32_t)(w - 0xD800) << 10) | (uint32_t)(w2 - 0xDC00));
                i += 2;
            }
        }

        o = s_utf8_write_cp(out, out_len, o, cp);
        if (o == 0 && out[0] == '\0') break;
    }

    s_trim_inplace(out);
}

static void s_decode_id3_text(uint8_t enc, const uint8_t *buf, size_t buf_len, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if (!buf || buf_len == 0) return;

    /* enc=0 ISO-8859-1, enc=3 UTF-8 (treat as C-string) */
    if (enc == 0 || enc == 3) {
        size_t n = 0;
        while (n < buf_len && buf[n] != 0) n++;
        if (n >= out_len) n = out_len - 1;
        memcpy(out, buf, n);
        out[n] = '\0';
        s_trim_inplace(out);
        return;
    }

    /* enc=1 UTF-16 with BOM */
    if (enc == 1) {
        if (buf_len < 2) return;
        bool be = false;
        const uint8_t *p = buf;
        size_t n = buf_len;

        if (buf[0] == 0xFE && buf[1] == 0xFF) { be = true;  p += 2; n -= 2; }
        else if (buf[0] == 0xFF && buf[1] == 0xFE) { be = false; p += 2; n -= 2; }

        s_utf16_to_utf8(p, n, be, out, out_len);
        return;
    }

    /* enc=2 UTF-16BE (no BOM) */
    if (enc == 2) {
        s_utf16_to_utf8(buf, buf_len, true, out, out_len);
        return;
    }

    /* unknown encoding: best effort */
    size_t n = (buf_len < out_len - 1) ? buf_len : (out_len - 1);
    memcpy(out, buf, n);
    out[n] = '\0';
    s_trim_inplace(out);
}

static bool s_get_full_path_by_index(size_t index, char *out, size_t out_len)
{
    if (!s_file_iterator || !out || out_len == 0) return false;

    size_t track_cnt = gui_hal_get_track_count();
    if (index >= track_cnt) return false;

    int n = file_iterator_get_full_path_from_index(s_file_iterator, (int)index, out, out_len);
    if (n <= 0 || (size_t)n >= out_len) return false;

    out[out_len - 1] = '\0';
    return true;
}

static bool s_parse_id3v1(FILE *fp, char *artist, size_t artist_len, char *album, size_t album_len)
{
    if (!fp) return false;

    long cur = ftell(fp);
    if (cur < 0) cur = 0;

    if (fseek(fp, 0, SEEK_END) != 0) return false;
    long sz = ftell(fp);
    if (sz < 128) { (void)fseek(fp, cur, SEEK_SET); return false; }

    if (fseek(fp, -128, SEEK_END) != 0) { (void)fseek(fp, cur, SEEK_SET); return false; }

    uint8_t tag[128];
    if (fread(tag, 1, sizeof(tag), fp) != sizeof(tag)) { (void)fseek(fp, cur, SEEK_SET); return false; }
    (void)fseek(fp, cur, SEEK_SET);

    if (memcmp(tag, "TAG", 3) != 0) return false;

    bool any = false;

    if (artist && artist_len > 0 && artist[0] == '\0') {
        char tmp[31] = {0};
        memcpy(tmp, &tag[33], 30);
        tmp[30] = '\0';
        s_trim_inplace(tmp);
        if (tmp[0]) { strncpy(artist, tmp, artist_len - 1); artist[artist_len - 1] = '\0'; any = true; }
    }

    if (album && album_len > 0 && album[0] == '\0') {
        char tmp[31] = {0};
        memcpy(tmp, &tag[63], 30);
        tmp[30] = '\0';
        s_trim_inplace(tmp);
        if (tmp[0]) { strncpy(album, tmp, album_len - 1); album[album_len - 1] = '\0'; any = true; }
    }

    return any;
}


static bool s_parse_id3v2(FILE *fp, char *artist, size_t artist_len, char *album, size_t album_len)
{
    if (!fp) return false;

    uint8_t hdr[10];
    if (fseek(fp, 0, SEEK_SET) != 0) return false;
    if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) return false;
    if (memcmp(hdr, "ID3", 3) != 0) return false;

    uint8_t ver_major = hdr[3];
    uint8_t flags = hdr[5];
    uint32_t tag_size = s_synchsafe32(&hdr[6]);
    uint32_t tag_end = 10U + tag_size;

    /* Skip extended header if present */
    if (flags & 0x40) {
        uint8_t szb[4];
        if (fread(szb, 1, 4, fp) != 4) return false;

        uint32_t ext_size = 0;
        if (ver_major >= 4) {
            ext_size = s_synchsafe32(szb);
            if (ext_size >= 4) (void)fseek(fp, (long)(ext_size - 4), SEEK_CUR);
        } else {
            ext_size = s_be32(szb);
            (void)fseek(fp, (long)ext_size, SEEK_CUR);
        }
    }

    bool any = false;

    while ((uint32_t)ftell(fp) + 10U <= tag_end) {
        uint8_t fh[10];
        if (fread(fh, 1, 10, fp) != 10) break;

        /* padding or invalid */
        if (fh[0] == 0 || fh[1] == 0 || fh[2] == 0 || fh[3] == 0) break;

        char id[5];
        memcpy(id, fh, 4);
        id[4] = '\0';

        uint32_t frame_size = (ver_major >= 4) ? s_synchsafe32(&fh[4]) : s_be32(&fh[4]);
        if (frame_size == 0) continue;

        uint32_t cur = (uint32_t)ftell(fp);
        if (cur + frame_size > tag_end) break;

        bool want_artist = (strcmp(id, "TPE1") == 0);
        bool want_album  = (strcmp(id, "TALB") == 0);

        if (!want_artist && !want_album) {
            (void)fseek(fp, (long)frame_size, SEEK_CUR);
            continue;
        }

        uint8_t enc = 0;
        if (fread(&enc, 1, 1, fp) != 1) break;
        frame_size--;

        uint8_t buf[256];
        size_t to_read = frame_size;
        if (to_read > sizeof(buf)) to_read = sizeof(buf);

        size_t got = fread(buf, 1, to_read, fp);
        if (frame_size > got) (void)fseek(fp, (long)(frame_size - got), SEEK_CUR);

        if (want_artist && artist && artist_len > 0 && artist[0] == '\0') {
            char tmp[GUI_HAL_ID3_MAX_TEXT] = {0};
            s_decode_id3_text(enc, buf, got, tmp, sizeof(tmp));
            if (tmp[0]) { strncpy(artist, tmp, artist_len - 1); artist[artist_len - 1] = '\0'; any = true; }
        }

        if (want_album && album && album_len > 0 && album[0] == '\0') {
            char tmp[GUI_HAL_ID3_MAX_TEXT] = {0};
            s_decode_id3_text(enc, buf, got, tmp, sizeof(tmp));
            if (tmp[0]) { strncpy(album, tmp, album_len - 1); album[album_len - 1] = '\0'; any = true; }
        }

        if ((!artist || artist[0]) && (!album || album[0])) break;
    }

    return any;
}

/**
 * @brief Read ID3 artist/album by track index
 * @param index  Track index in current iterator
 * @param artist Output artist buffer (can be NULL)
 * @param artist_len artist buffer length
 * @param album  Output album buffer (can be NULL)
 * @param album_len  album buffer length
 * @return true if any tag was found (artist or album), false otherwise
 */
bool gui_hal_get_artist_album_by_index(size_t index,
                                      char *artist, size_t artist_len,
                                      char *album,  size_t album_len)
{
    if (artist && artist_len > 0) artist[0] = '\0';
    if (album  && album_len  > 0) album[0]  = '\0';

    char full_path[GUI_HAL_MAX_PATH_LEN] = {0};
    if (!s_get_full_path_by_index(index, full_path, sizeof(full_path))) {
        return false;
    }

    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        ESP_LOGW(TAG, "gui_hal_get_artist_album_by_index: open failed: %s", full_path);
        return false;
    }

    bool any = false;

    if (s_parse_id3v2(fp, artist, artist_len, album, album_len)) any = true;
    if (s_parse_id3v1(fp, artist, artist_len, album, album_len)) any = true;

    fclose(fp);

    if (artist) s_trim_inplace(artist);
    if (album)  s_trim_inplace(album);

    return any;
}
/**
 * @brief Check if a file path has a case-insensitive ".mp3" extension
 * @param path Null-terminated string containing the file path to check
 * @return true if path is a valid MP3 file, false otherwise (null path/no extension/invalid extension)
 */
static bool s_is_mp3_file(const char *path)
{
    if (!path) {
        return false;
    }

    /* Find last occurrence of '.' to locate file extension */
    const char *dot = strrchr(path, '.');
    if (!dot) {
        return false;
    }

    /* Skip the '.' character to point to extension start */
    dot++;

    /* Validate extension length (must be exactly 3 characters: "mp3") */
    if (!dot[0] || !dot[1] || !dot[2] || dot[3] != '\0') {
        return false;
    }

    /* Convert extension characters to lowercase for case-insensitive check */
    char c0 = dot[0];
    char c1 = dot[1];
    char c2 = dot[2];

    if (c0 >= 'A' && c0 <= 'Z') c0 = (char)(c0 + ('a' - 'A'));
    if (c1 >= 'A' && c1 <= 'Z') c1 = (char)(c1 + ('a' - 'A'));
    if (c2 >= 'A' && c2 <= 'Z') c2 = (char)(c2 + ('a' - 'A'));

    /* Check if extension is exactly "mp3" */
    return (c0 == 'm' && c1 == 'p' && c2 == '3');
}

/* -------------------------------------------------------------------------- */
/* Audio smoothing helpers (click/pop suppression)                             */
/* -------------------------------------------------------------------------- */

static inline uint32_t s_ms_to_frames(uint32_t ms)
{
    uint32_t rate = s_pcm_rate_hz ? s_pcm_rate_hz : 44100U;
    return (uint32_t)(((uint64_t)rate * (uint64_t)ms) / 1000ULL);
}

/* Request N ms of strict zero output at the beginning of the next writes. */
static void s_request_start_silence(uint32_t ms)
{
    uint32_t frames = s_ms_to_frames(ms);
    if (frames == 0U) frames = 1U;
    s_force_silence_frames = frames;
}

/* Prepare a soft ramp-in from silence for a (re)start. */
static void s_prepare_fade_in(void)
{
    s_gain_q15 = 0;
    s_gain_target_q15 = (s_soft_mute || (s_soft_volume == 0U)) ? 0 : 32768;
    s_request_start_silence(GUI_HAL_START_SILENCE_MS);
}

/* Prepare a soft ramp-in for RESUME (do NOT overwrite samples with forced zeros). */
static void s_prepare_resume_fade_in(void)
{
    s_gain_q15 = 0;
    s_gain_target_q15 = (s_soft_mute || (s_soft_volume == 0U)) ? 0 : 32768;
    /* Keep s_force_silence_frames unchanged (do not blank audio on resume). */
}

/* Note: I2S DMA silence flush is now provided by board_speaker.c (speaker_flush_silence_ms). */

static void s_play_index(size_t index)
{
    if (!s_file_iterator) {
        ESP_LOGW(TAG, "s_play_index: file iterator not initialized");
        return;
    }

    /* Validate index against iterator range */
    size_t iter_cnt = file_iterator_get_count(s_file_iterator);
    if (index >= iter_cnt) {
        ESP_LOGW(TAG,
                 "s_play_index: index %zu out of iterator range (count=%zu)",
                 index, iter_cnt);
        return;
    }

    ESP_LOGI(TAG, "s_play_index(%zu)", index);

    /* Retrieve full path of the track from iterator */
    char filename[GUI_HAL_MAX_PATH_LEN];
    int retval = file_iterator_get_full_path_from_index(
        s_file_iterator,
        (int)index,
        filename,
        sizeof(filename)
    );
    if (retval <= 0) {
        ESP_LOGE(TAG,
                 "s_play_index: unable to retrieve filename for index %zu",
                 index);
        return;
    }

    /* Skip non-MP3 files */
    if (!s_is_mp3_file(filename)) {
        ESP_LOGW(TAG,
                 "s_play_index: skip non-mp3 file (index=%zu, name='%s')",
                 index,
                 filename);
        return;
    }

    /* Open file in read-only binary mode (ownership transferred to audio player) */
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        ESP_LOGE(TAG,
                 "s_play_index: unable to open index %zu, filename '%s'",
                 index,
                 filename);
        return;
    }

    ESP_LOGI(TAG, "Playing '%s'", filename);

    /* If we are starting from IDLE/PAUSE, prepare a clean ramp-in to avoid pop. */
    audio_player_state_t st = audio_player_get_state();
    if (st != AUDIO_PLAYER_STATE_PLAYING) {
        s_prepare_fade_in();
    }

    audio_player_play(fp);
}

/* -------------------------------------------------------------------------- */
/* Public Track Info / Scan Functions (API)                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get the number of scanned music tracks (clipped to MAX_MP3_NUM)
 * @return Number of valid music tracks (0 if no scan performed/failed)
 */
size_t gui_hal_get_track_count(void)
{
    return s_track_count;
}

/**
 * @brief Scan the /sdcard/music directory for music files (MP3)
 * @details Initializes the file iterator, counts files in the target directory,
 *          and clips the count to MAX_MP3_NUM to match system limits.
 * @note Overwrites existing iterator and track count on each call
 */
esp_err_t  gui_hal_music_scan(void)
{
    /* Reset state before new scan */
    s_file_iterator = NULL;
    s_track_count = 0U;

    struct stat st;
    if (stat("/sdcard/music", &st) != 0 || !S_ISDIR(st.st_mode)) {
        ESP_LOGW(TAG, "music folder not found: /sdcard/music");
        return ESP_ERR_NOT_FOUND;  
    }


    /* Create new file iterator for music directory */
    s_file_iterator = file_iterator_new("/sdcard/music");
    if (!s_file_iterator) {
        ESP_LOGE(TAG, "file_iterator_new(\"/sdcard/music\") failed");
        return ESP_FAIL;
    }

    /* Get total file count from iterator */
    s_track_count = file_iterator_get_count(s_file_iterator);
    
    /* Clip count to system maximum if exceeded */
    if (s_track_count > MAX_MP3_NUM) {
        ESP_LOGW(TAG,
                 "track count %zu > MAX_MP3_NUM=%u, clipping",
                 s_track_count,
                 (unsigned)MAX_MP3_NUM);
        s_track_count = MAX_MP3_NUM;
    }

    ESP_LOGI(TAG, "found %zu files in music directory", s_track_count);
    return ESP_OK;

}

/**
 * @brief Get estimated track length (seconds) from file size
 * @details Calculates approximate duration using file size and nominal I2S frequency
 *          (very rough estimate, not based on MP3 metadata)
 * @param index Zero-based index of the track to query
 * @return Estimated track length in seconds (0 if invalid index/iterator/stat failure)
 * 
 * 
 * @note This is a crude estimate based solely on file size and I2S_FRE and
 *       does not use MP3 metadata. It is intended for UI purposes only.
 */
uint32_t gui_hal_get_track_length(size_t index)
{
    if (!s_file_iterator) {
        ESP_LOGW(TAG, "gui_hal_get_track_length: file iterator not initialized");
        return 0;
    }

    size_t track_cnt = gui_hal_get_track_count();
    if (index >= track_cnt) {
        return 0;
    }

    /* Retrieve full path of the track */
    char full_path[GUI_HAL_MAX_PATH_LEN] = {0};
    int n = file_iterator_get_full_path_from_index(
        s_file_iterator,
        (int)index,
        full_path,
        sizeof(full_path)
    );
    if (n <= 0 || (size_t)n >= sizeof(full_path)) {
        ESP_LOGE(TAG,
                 "gui_hal_get_track_length: failed to get path for index %zu",
                 index);
        return 0;
    }

    /* Use stat() for file size (faster than fseek+ftell on many FS) */
    struct stat st;
    if (stat(full_path, &st) != 0) {
        ESP_LOGE(TAG, "gui_hal_get_track_length: stat failed for %s", full_path);
        return 0;
    }
    if (st.st_size <= 0) {
        return 0;
    }
    const uint64_t file_size = (uint64_t)st.st_size;

    FILE* file = fopen(full_path, "rb");
    if (!file) {
        ESP_LOGE(TAG, "gui_hal_get_track_length: failed to open file: %s", full_path);
        return 0;
    }

    /* 1) Skip possible ID3v2 tag (syncsafe size) */
    uint64_t audio_start_offset = 0;
    uint8_t id3_header[10];
    if (fread(id3_header, 1, 10, file) == 10) {
        if (memcmp(id3_header, "ID3", 3) == 0) {
            uint32_t tag_size =
                ((uint32_t)(id3_header[6] & 0x7F) << 21) |
                ((uint32_t)(id3_header[7] & 0x7F) << 14) |
                ((uint32_t)(id3_header[8] & 0x7F) << 7)  |
                ((uint32_t)(id3_header[9] & 0x7F));
            audio_start_offset = 10ULL + (uint64_t)tag_size;
        }
    }

    if (audio_start_offset >= file_size) {
        fclose(file);
        return 0;
    }

    /* 2) Read a chunk into memory and find first valid MP3 frame header */
    if (fseek(file, (long)audio_start_offset, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

 
    int rd = (int)fread(mp3_buf, 1, BUF_SZ, file);
    fclose(file);

    if (rd < 4) {
        return 0;
    }

    int pos = -1;
    for (int i = 0; i + 4 <= rd; i++) {
        if (mp3_buf[i] == 0xFF && (mp3_buf[i + 1] & 0xE0) == 0xE0) {
            if (drmp3_hdr_valid(&mp3_buf[i])) {
                pos = i;
                break;
            }
        }
    }

    uint64_t audio_bytes = file_size - audio_start_offset;

    /* If we cannot find header quickly, fallback to fast estimate */
    if (pos < 0) {
        uint32_t sec = (uint32_t)((audio_bytes * 8ULL) / 128000ULL);
        if (sec == 0) sec = 1;
        ESP_LOGW(TAG, "MP3 length fallback estimate (no header found): %u sec", (unsigned)sec);
        return sec;
    }

    const uint8_t* hdr = &mp3_buf[pos];
    int bitrate_kbps = drmp3_hdr_bitrate_kbps(hdr);
    int sample_rate  = drmp3_hdr_sample_rate_hz(hdr);

    if (bitrate_kbps <= 0 || sample_rate <= 0) {
        uint32_t sec = (uint32_t)((audio_bytes * 8ULL) / 128000ULL);
        if (sec == 0) sec = 1;
        ESP_LOGW(TAG, "MP3 length fallback estimate (bad hdr): %u sec", (unsigned)sec);
        return sec;
    }

    /* Helpers for reading big-endian u32 from mp3_buf */
    #define BE32(p) ( ((uint32_t)(p)[0] << 24) | ((uint32_t)(p)[1] << 16) | ((uint32_t)(p)[2] << 8) | (uint32_t)(p)[3] )

    /* 3) Try Xing/Info or VBRI for VBR accurate duration (very fast) */
    uint32_t duration_sec = 0;

    // MPEG version: (hdr[1] >> 3) & 3; 3=MPEG1, 2=MPEG2, 0=MPEG2.5
    int ver = (hdr[1] >> 3) & 0x03;
    bool mpeg1 = (ver == 0x03);

    // protection bit: 1=no CRC, 0=has CRC
    bool has_crc = ((hdr[1] & 0x01) == 0);

    // channel mode: (hdr[3] >> 6) & 3; 3=mono
    bool mono = (((hdr[3] >> 6) & 0x03) == 0x03);

    // Layer III side info bytes
    int sideinfo = 0;
    if (mpeg1) sideinfo = mono ? 17 : 32;
    else       sideinfo = mono ?  9 : 17;

    // Samples per frame for Layer III: MPEG1=1152, MPEG2/2.5=576
    int samples_per_frame = mpeg1 ? 1152 : 576;

    // Xing/Info offset = frame_header(4) + CRC(0/2) + sideinfo
    int xing_off = pos + 4 + (has_crc ? 2 : 0) + sideinfo;
    if (xing_off + 16 < rd) {
        const uint8_t* p = &mp3_buf[xing_off];
        if (memcmp(p, "Xing", 4) == 0 || memcmp(p, "Info", 4) == 0) {
            uint32_t flags = BE32(p + 4);
            uint32_t frames = 0;
            int off = 8;
            if (flags & 0x00000001) { // frames present
                frames = BE32(p + off);
            }
            if (frames > 0) {
                double sec = (double)frames * (double)samples_per_frame / (double)sample_rate;
                duration_sec = (uint32_t)(sec + 0.5);
            }
        }
    }

    // VBRI header commonly at frame_header(4)+CRC(0/2)+32
    if (duration_sec == 0) {
        int vbri_off = pos + 4 + (has_crc ? 2 : 0) + 32;
        if (vbri_off + 26 < rd) {
            const uint8_t* p = &mp3_buf[vbri_off];
            if (memcmp(p, "VBRI", 4) == 0) {
                // frames count often at offset 14
                uint32_t frames = BE32(p + 14);
                if (frames > 0) {
                    double sec = (double)frames * (double)samples_per_frame / (double)sample_rate;
                    duration_sec = (uint32_t)(sec + 0.5);
                }
            }
        }
    }

    /* 4) If no VBR header, use fast CBR/approx estimate */
    if (duration_sec == 0) {
        double sec = (audio_bytes * 8.0) / ((double)bitrate_kbps * 1000.0);
        duration_sec = (uint32_t)(sec + 0.5);
    }

    if (duration_sec == 0) duration_sec = 1;

    ESP_LOGD(TAG,
             "MP3 len: ssid? index %zu, size=%llu, bitrate=%d kbps, sr=%d Hz, len=%u s",
             index,
             (unsigned long long)file_size,
             bitrate_kbps,
             sample_rate,
             (unsigned)duration_sec);

    return duration_sec;

    #undef BE32
}

/**
 * @brief Get track title (filename without path/extension) by index
 * @param index Zero-based index of the track to query
 * @param mp3_buf Output buffer to store the title (null-terminated string)
 * @param buf_len Length of the output buffer (including null terminator)
 * @return true if title was successfully retrieved, false otherwise (invalid index/iterator/buffer)
 */
bool gui_hal_get_title_by_index(size_t index, char *mp3_buf, size_t buf_len)
{
    if (!s_file_iterator) {
        ESP_LOGW(TAG, "gui_hal_get_title_by_index: file iterator not initialized");
        return false;
    }
    if (!mp3_buf || buf_len == 0U) {
        return false;
    }

    size_t track_cnt = gui_hal_get_track_count();
    if (index >= track_cnt) {
        return false;
    }

    /* Retrieve full path of the track */
    char full_path[GUI_HAL_MAX_PATH_LEN] = {0};
    int n = file_iterator_get_full_path_from_index(
        s_file_iterator,
        (int)index,
        full_path,
        sizeof(full_path)
    );
    if (n <= 0 || (size_t)n >= sizeof(full_path)) {
        return false;
    }

    /* Extract filename from full path (strip directory) */
    char *fname = strrchr(full_path, '/');
    fname = fname ? fname + 1 : full_path;

    /* Copy filename to output buffer (ensure null termination) */
    strncpy(mp3_buf, fname, buf_len - 1U);
    mp3_buf[buf_len - 1U] = '\0';

    /* Strip file extension (everything after last '.') */
    char *dot = strrchr(mp3_buf, '.');
    if (dot) {
        *dot = '\0';
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/* Volume Control Functions (API)                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set software volume level
 * @param vol Desired volume level (0 = mute, 100 = max; values >100 are clamped to 100)
 * @note Does not directly control hardware volume (applied in audio write callback)
 */
void gui_hal_set_volume(uint8_t vol)
{
    if (vol > 100U) {
        vol = 100U;
    }
    s_soft_volume = vol;
}

/**
 * @brief Get current software volume level
 * @return Current volume level (0-100)
 */
uint8_t gui_hal_get_volume(void)
{
    return s_soft_volume;
}

/* -------------------------------------------------------------------------- */
/* Audio Player Callback Functions (Private)                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Mute control callback for audio player
 * @details Updates the software mute flag (hardware mute can be added here)
 * @param setting Mute setting (AUDIO_PLAYER_MUTE = mute, AUDIO_PLAYER_UNMUTE = unmute)
 * @return ESP_OK on success (always returns OK in current implementation)
 */
static esp_err_t s_audio_player_mute_cb(AUDIO_PLAYER_MUTE_SETTING setting)
{
    /* Note: esp-audio-player uses mute_fn for internal transitions (start/stop),
       so keep it "soft" to avoid click/pop. */
    ESP_LOGI(TAG, "s_audio_player_mute_cb setting=%d", setting);

    s_soft_mute = (setting == AUDIO_PLAYER_MUTE);
    s_gain_target_q15 = s_soft_mute ? 0 : 32768;

    /* When muting, also force a short silence window to guarantee a hard zero boundary. */
    if (s_soft_mute) {
        s_gain_q15 = 0;
        s_request_start_silence(8U);
    }
    return ESP_OK;
}

/**
 * @brief Audio write callback for audio player (applies volume/mute to PCM data)
 * @details Processes PCM samples (applies volume scaling or mute), then forwards
 *          the data to the speaker hardware driver.
 * @param audio_buffer Pointer to PCM audio sample buffer (16-bit signed integers)
 * @param len Length of the buffer in bytes (must be multiple of 2 for 16-bit samples)
 * @param bytes_written Output: number of bytes actually written to speaker
 * @param timeout_ms Timeout (ms) for speaker write operation
 * @return ESP_OK on success, ESP_ERR_* on failure (from speaker_play)
 */
static esp_err_t s_audio_player_write_cb(void *audio_buffer,
                                         size_t len,
                                         size_t *bytes_written,
                                         uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;

    /* Cast buffer to 16-bit PCM samples (standard for audio playback) */
    int16_t *samples = (int16_t *)audio_buffer;

    uint32_t ch = (s_pcm_channels == 1U) ? 1U : 2U;
    size_t bytes_per_frame = ch * sizeof(int16_t);
    if (bytes_per_frame == 0U) {
        return ESP_FAIL;
    }

    size_t frames = len / bytes_per_frame;
    if (frames == 0U) {
        if (bytes_written) *bytes_written = 0U;
        return ESP_OK;
    }

    /* 0) Optional strict silence window (used for clean restarts) */
    if (s_force_silence_frames > 0U) {
        memset(samples, 0, frames * bytes_per_frame);

        if (s_force_silence_frames > frames) {
            s_force_silence_frames -= (uint32_t)frames;
        } else {
            s_force_silence_frames = 0U;
        }

        /* Keep gain parked at 0 during forced silence */
        s_gain_q15 = 0;

        ret = speaker_play(audio_buffer, frames * bytes_per_frame, bytes_written, timeout_ms);
        return ret;
    }

    /* 1) Compute ramp step (per frame) */
    bool muted = s_soft_mute || (s_soft_volume == 0U);
    int32_t target = muted ? 0 : s_gain_target_q15;

    int32_t g = s_gain_q15;
    int32_t step = 0;

    if (g != target) {
        uint32_t rate = s_pcm_rate_hz ? s_pcm_rate_hz : 44100U;
        uint32_t fade_frames = (uint32_t)(((uint64_t)rate * (uint64_t)GUI_HAL_FADE_MS) / 1000ULL);
        if (fade_frames == 0U) fade_frames = 1U;

        int32_t step_mag = 32768 / (int32_t)fade_frames;
        if (step_mag < 1) step_mag = 1;

        step = (target > g) ? step_mag : -step_mag;
    }

    /* 2) Apply volume + ramp. Gain is applied per FRAME so both channels keep same gain. */
    for (size_t f = 0; f < frames; ++f) {
        int32_t gf = g + (int32_t)f * step;
        if (step > 0 && gf > target) gf = target;
        if (step < 0 && gf < target) gf = target;

        /* Combine user volume (0..100) with ramp gain (Q15) into a Q15 scaler */
        int32_t vol_q15 = (muted) ? 0 : ((int32_t)s_soft_volume * gf) / 100;
        if (vol_q15 < 0) vol_q15 = 0;
        if (vol_q15 > 32768) vol_q15 = 32768;

        size_t base = f * ch;
        for (uint32_t c = 0; c < ch; ++c) {
            int32_t v = samples[base + c];
            v = (v * vol_q15) >> 15;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            samples[base + c] = (int16_t)v;
        }
    }

    /* 3) Update current gain for next callback */
    int32_t g_end = g + (int32_t)frames * step;
    if (step > 0 && g_end > target) g_end = target;
    if (step < 0 && g_end < target) g_end = target;
    s_gain_q15 = g_end;

    /* Forward processed samples to speaker driver */
    ret = speaker_play(audio_buffer, frames * bytes_per_frame, bytes_written, timeout_ms);
    return ret;
}

/**
 * @brief Clock configuration callback for audio player
 * @details Logs requested audio parameters (can be extended to configure codec/I2S hardware)
 * @param rate Audio sample rate (Hz)
 * @param bits_cfg Audio bit configuration (e.g., 16-bit PCM)
 * @param ch I2S channel mode (mono/stereo)
 * @return ESP_OK on success (always returns OK in current implementation)
 * @note Add bsp_codec_set_fs() here to configure hardware codec if needed
 */
static esp_err_t s_audio_player_clock_cb(uint32_t rate,
                                         uint32_t bits_cfg,
                                         i2s_slot_mode_t ch)
{
    ESP_LOGI(TAG,
             "s_audio_player_clock_cb rate=%lu bits_cfg=%lu",
             (unsigned long)rate,
             (unsigned long)bits_cfg);

    /* Cache runtime format for smoothing helpers. */
    if (rate != 0U) {
        s_pcm_rate_hz = rate;
    }
    /* bits_cfg is typically 16 or 32 */
    if (bits_cfg == 32U) {
        s_pcm_bits = 32U;
    } else {
        s_pcm_bits = 16U;
    }
    s_pcm_channels = (ch == I2S_SLOT_MODE_MONO) ? 1U : 2U;

    /* Pass format to speaker helpers (used for silence flush). */
    speaker_set_pcm_format(s_pcm_rate_hz, s_pcm_bits, s_pcm_channels);

    /* Example: configure codec here if required */
    /* bsp_codec_set_fs(rate, bits_cfg, ch); */
    return ESP_OK;
}

/**
 * @brief Fetch a track end/auto-next index from the FreeRTOS queue (non-blocking)
 * @details Used by GUI task to get notified of track end events from audio player
 * @param p_index Output: pointer to store the track index (valid only if return is true)
 * @return true if an index was retrieved from the queue, false otherwise (empty queue/invalid params)
 */
bool gui_hal_fetch_track_end(uint32_t *p_index)
{
    if (!s_track_end_queue || !p_index) {
        return false;
    }

    uint32_t idx = 0;
    /* Non-blocking read from queue (0 timeout) */
    if (xQueueReceive(s_track_end_queue, &idx, 0) == pdPASS) {
        *p_index = idx;
        return true;
    }
    return false;
}

/**
 * @brief Main event callback for audio player (handles playback state changes)
 * @details Triggers auto-next track on IDLE event, controls speaker power, and sends
 *          track end events to GUI via queue.
 * @param ctx Pointer to audio player callback context (contains event type)
 * @note Called from audio player task (FreeRTOS)
 */
static void s_audio_player_event_cb(audio_player_cb_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    ESP_LOGI(TAG, "audio_event=%d", ctx->audio_event);

    switch (ctx->audio_event) {
    case AUDIO_PLAYER_CALLBACK_EVENT_IDLE: {
        /* Playback completed: turn off speaker and trigger auto-next */
        ESP_LOGI(TAG, "AUDIO_PLAYER_CALLBACK_EVENT_IDLE");
        ctrl_speaker_off();

        if (!s_file_iterator) {
            ESP_LOGW(TAG, "IDLE: file iterator is NULL");
            break;
        }

        /* Auto-next: increment iterator and play new track (skips non-MP3 files) */
        file_iterator_next(s_file_iterator);
        int index = file_iterator_get_index(s_file_iterator);
        ESP_LOGI(TAG, "IDLE: auto-next index=%d", index);

        /* Smooth start for auto-next */
        s_prepare_fade_in();
        s_play_index((size_t)index);

        /* Send auto-next index to GUI task via queue (non-blocking) */
        if (s_track_end_queue) {
            uint32_t idx_u32 = (uint32_t)index;
            (void)xQueueSend(s_track_end_queue, &idx_u32, 0);
        }
        break;
    }

    case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
        /* Playback started: turn on speaker */
        ESP_LOGI(TAG, "AUDIO_PLAYER_CALLBACK_EVENT_PLAYING");
        ctrl_speaker_on();
        break;

    case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
        /* Playback paused: park I2S output at 0 to avoid click when switching tracks from PAUSE */
        ESP_LOGI(TAG, "AUDIO_PLAYER_CALLBACK_EVENT_PAUSE");
        s_gain_q15 = 0;
        speaker_flush_silence_ms(GUI_HAL_PAUSE_FLUSH_MS);
        /* Keep amplifier/I2S enabled during PAUSE to ensure RESUME has audio.
           Output is already parked at 0, so it remains silent without hard power toggles. */
        break;

    default:
        /* Ignore unhandled events */
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* UI Control Functions (API)                                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Toggle play/pause state of audio playback
 * @details Starts playback (if idle), resumes (if paused), or pauses (if playing)
 * @note No-op if file iterator is not initialized (no tracks scanned)
 */
void gui_hal_toggle_play_pause(void)
{
    audio_player_state_t state = audio_player_get_state();
    ESP_LOGI(TAG, "gui_hal_toggle_play_pause: state=%d", state);

    if (state == AUDIO_PLAYER_STATE_IDLE) {
        /* Idle: start playback of current iterator index */
        if (!s_file_iterator) {
            ESP_LOGW(TAG, "gui_hal_toggle_play_pause: file iterator not initialized");
            return;
        }

        int index = file_iterator_get_index(s_file_iterator);
        ESP_LOGI(TAG, "gui_hal_toggle_play_pause: starting index=%d", index);
        s_prepare_fade_in();
        s_play_index((size_t)index);

    } else if (state == AUDIO_PLAYER_STATE_PAUSE) {
        /* Paused: resume playback */
        /* Restore target gain and ramp-in smoothly on resume (no forced zero overwrite). */
        ctrl_speaker_on();
        s_prepare_resume_fade_in();
        audio_player_resume();

    } else if (state == AUDIO_PLAYER_STATE_PLAYING) {
        /* Playing: pause playback */
        audio_player_pause();
    }
}

/**
 * @brief Navigate to previous/next track and update playback
 * @param is_next true = next track, false = previous track
 * @details Updates file iterator position and restarts playback (if active)
 * @note Does not start playback if audio player is idle
 */
void gui_hal_prev_next(uint8_t is_next)
{
    if (!s_file_iterator) {
        ESP_LOGW(TAG, "gui_hal_prev_next: file iterator not initialized");
        return;
    }

    /* Update iterator position */
    if (is_next) {
        ESP_LOGI(TAG, "gui_hal_prev_next: next");
        file_iterator_next(s_file_iterator);
    } else {
        ESP_LOGI(TAG, "gui_hal_prev_next: prev");
        file_iterator_prev(s_file_iterator);
    }

    /* Get new index and current playback state */
    int index = file_iterator_get_index(s_file_iterator);
    audio_player_state_t state = audio_player_get_state();

    /* Update playback based on state */
    if (state == AUDIO_PLAYER_STATE_IDLE) {
        /* Idle: do not start playback automatically */
        return;
    } else if (state == AUDIO_PLAYER_STATE_PAUSE) {
        /* Paused: load new track and keep paused */
        s_play_index((size_t)index);
        audio_player_pause();
    } else if (state == AUDIO_PLAYER_STATE_PLAYING) {
        /* Playing: load and play new track */
        s_play_index((size_t)index);
    }
}

/**
 * @brief Play a specific track by index (e.g., from GUI list selection)
 * @details Synchronizes file iterator to target index (optimized for shortest path),
 *          then starts playback of the selected track.
 * @param index Zero-based index of the track to play (must be < track count)
 * @note No-op if index is out of range or iterator is not initialized
 */
void gui_hal_play_index(size_t index)
{
    if (!s_file_iterator) {
        ESP_LOGW(TAG, "gui_hal_play_index: file iterator not initialized");
        return;
    }

    size_t track_cnt = gui_hal_get_track_count();
    if (index >= track_cnt) {
        ESP_LOGW(TAG,
                 "gui_hal_play_index: index %zu out of range (count=%zu)",
                 index,
                 track_cnt);
        return;
    }

    /* Sync iterator to target index (shortest path: forward or backward) */
    size_t cur = (size_t)file_iterator_get_index(s_file_iterator);
    if (cur != index) {
        size_t n        = track_cnt;
        size_t forward  = (index + n - cur) % n;  /* Steps to move forward */
        size_t backward = (cur + n - index) % n; /* Steps to move backward */

        /* Choose shortest path to target index */
        if (forward <= backward) {
            for (size_t i = 0; i < forward; ++i) {
                file_iterator_next(s_file_iterator);
            }
        } else {
            for (size_t i = 0; i < backward; ++i) {
                file_iterator_prev(s_file_iterator);
            }
        }
    }

    int idx_now = file_iterator_get_index(s_file_iterator);
    ESP_LOGI(TAG,
             "gui_hal_play_index: iterator index=%d, target=%zu",
             idx_now,
             index);

    /* If selecting from PAUSE/IDLE, start from silence then ramp up to avoid click. */
    audio_player_state_t st = audio_player_get_state();
    if (st == AUDIO_PLAYER_STATE_PAUSE || st == AUDIO_PLAYER_STATE_IDLE) {
        s_prepare_fade_in();
    }

    /* Play the target track */
    s_play_index(index);
}

/* -------------------------------------------------------------------------- */
/* Initialization Function (API)                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize GUI HAL audio subsystem
 * @details Configures audio player callbacks, creates track end queue,
 *          initializes audio player, and sets default volume (20%).
 * @note Must be called before any other audio-related GUI HAL functions
 */
void gui_hal_audio_init(void)
{
    /* Configure audio player callbacks and task parameters */
    s_player_config.mute_fn    = s_audio_player_mute_cb;
    s_player_config.write_fn   = s_audio_player_write_cb;
    s_player_config.clk_set_fn = s_audio_player_clock_cb;
    s_player_config.priority   = 4;    /* FreeRTOS task priority */
    s_player_config.coreID     = 1;    /* Core to run audio player task on */

    /* Create FreeRTOS queue for track end events (if not already created) */
    if (!s_track_end_queue) {
        s_track_end_queue = xQueueCreate(GUI_HAL_TRACK_END_QUEUE_LEN,
                                         sizeof(uint32_t));
        if (!s_track_end_queue) {
            ESP_LOGE(TAG, "gui_hal_audio_init: failed to create track_end queue");
        }
    }

    /* Initialize audio player and register event callback */
    ESP_ERROR_CHECK(audio_player_new(s_player_config));
    ESP_ERROR_CHECK(audio_player_callback_register(s_audio_player_event_cb,NULL));
                                                   

    /* Set default volume (20% to avoid loud startup) */
    gui_hal_set_volume(20u);
}
