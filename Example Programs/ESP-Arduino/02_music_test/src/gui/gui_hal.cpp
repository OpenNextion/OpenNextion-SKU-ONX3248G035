#include "gui_hal.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <FS.h>
#include <Audio.h>
#include <ctype.h>
#include <string.h>

#include "../../BoardConfig.h"

static const char *MUSIC_DIR = "/music";
static String s_paths[MAX_MP3_NUM];
static String s_titles[MAX_MP3_NUM];
static size_t s_track_count = 0;
static size_t s_current_index = 0;
static bool s_playing = false;
static bool s_audio_ready = false;
static uint32_t s_amp_enable_at_ms = 0;

static Audio audio;

// Implemented by the sketch because PCF8574 state is shared with LCD reset/SD CS.
extern "C" void board_set_speaker_amp(bool enable);

static bool isMp3Name(const char *name) {
  if (!name) return false;
  const char *dot = strrchr(name, '.');
  if (!dot) return false;
  return strcasecmp(dot, ".mp3") == 0;
}

static String titleFromPath(const char *path) {
  String title(path ? path : "");
  int slash = title.lastIndexOf('/');
  if (slash >= 0) title.remove(0, slash + 1);
  int dot = title.lastIndexOf('.');
  if (dot > 0) title.remove(dot);
  if (title.length() == 0) title = "Unknown";
  return title;
}

extern "C" esp_err_t gui_hal_music_scan(void) {
  s_track_count = 0;
  File dir = SD_MMC.open(MUSIC_DIR);
  if (!dir || !dir.isDirectory()) {
    Serial.println("music folder not found: /music");
    return ESP_ERR_NOT_FOUND;
  }

  for (File f = dir.openNextFile(); f && s_track_count < MAX_MP3_NUM; f = dir.openNextFile()) {
    if (!f.isDirectory() && isMp3Name(f.name())) {
      String fullPath = String(MUSIC_DIR) + "/" + f.name();
      s_paths[s_track_count] = fullPath;
      s_titles[s_track_count] = titleFromPath(fullPath.c_str());
      s_track_count++;
    }
    f.close();
  }

  Serial.printf("found %u MP3 file(s) in /music\r\n", (unsigned)s_track_count);
  return s_track_count > 0 ? ESP_OK : ESP_ERR_NOT_FOUND;
}

extern "C" size_t gui_hal_get_track_count(void) {
  return s_track_count;
}

extern "C" bool gui_hal_get_title_by_index(size_t index, char *buf, size_t buf_len) {
  if (!buf || buf_len == 0 || index >= s_track_count) return false;
  strlcpy(buf, s_titles[index].c_str(), buf_len);
  return true;
}

extern "C" uint32_t gui_hal_get_track_length(size_t index) {
  if (index >= s_track_count) return 0;
  File f = SD_MMC.open(s_paths[index].c_str(), FILE_READ);
  if (!f) return 0;
  uint64_t bytes = f.size();
  f.close();
  // Keep this lightweight for UI purposes. The decoder still handles real VBR/CBR playback.
  uint32_t seconds = (bytes * 8ULL) / 128000ULL;
  return seconds > 0 ? seconds : 1;
}

extern "C" void gui_hal_play_index(size_t index) {
  if (index >= s_track_count) return;
  s_current_index = index;
  s_playing = false;
  s_amp_enable_at_ms = 0;
  // Avoid pops when switching tracks: mute the amplifier before stopping/starting I2S.
  board_set_speaker_amp(false);
  audio.stopSong();
  bool ok = audio.connecttoFS(SD_MMC, s_paths[index].c_str());
  Serial.printf("Play track: %s, %s\r\n", s_paths[index].c_str(), ok ? "OK" : "FAILED");
  s_playing = ok;
  if (ok) {
    // Give the decoder and I2S DMA a short lead-in before enabling NS4168.
    s_amp_enable_at_ms = millis() + 120;
  }
}

extern "C" bool gui_hal_fetch_track_end(uint32_t *p_index) {
  (void)p_index;
  return false;
}

extern "C" void gui_hal_set_volume(uint8_t vol) {
  // ESP-IDF UI uses 0-100; ESP32-audioI2S uses 0-21.
  uint8_t audioVol = map(constrain((int)vol, 0, 100), 0, 100, 0, 21);
  audio.setVolume(audioVol);
  Serial.printf("Volume: %u (%u/21)\r\n", (unsigned)vol, (unsigned)audioVol);
}

extern "C" uint8_t gui_hal_get_volume(void) {
  return 80;
}

extern "C" void gui_hal_toggle_play_pause(void) {
  if (s_track_count == 0) return;
  if (!s_audio_ready) return;

  if (!audio.isRunning() && !s_playing) {
    gui_hal_play_index(s_current_index);
    return;
  }

  bool ok = audio.pauseResume();
  s_playing = ok ? audio.isRunning() : s_playing;
  if (ok) {
    if (s_playing) {
      // Resume path: wait briefly before re-enabling the amplifier.
      s_amp_enable_at_ms = millis() + 80;
    } else {
      s_amp_enable_at_ms = 0;
      board_set_speaker_amp(false);
    }
  }
  Serial.printf("%s: %s\r\n", s_playing ? "Resume" : "Pause", s_paths[s_current_index].c_str());
}

extern "C" void gui_hal_prev_next(uint8_t is_next) {
  if (s_track_count == 0) return;
  bool restart = s_playing || audio.isRunning();
  if (is_next) {
    s_current_index = (s_current_index + 1) % s_track_count;
  } else {
    s_current_index = (s_current_index == 0) ? (s_track_count - 1) : (s_current_index - 1);
  }
  Serial.printf("Track switch: %s\r\n", s_paths[s_current_index].c_str());
  if (restart) {
    gui_hal_play_index(s_current_index);
  }
}

extern "C" void gui_hal_audio_init(void) {
  s_current_index = 0;
  s_playing = false;
  s_amp_enable_at_ms = 0;
  // The amplifier is intentionally left off until playback is stable.
  board_set_speaker_amp(false);
  audio.setPinout(SPEAKER_BCLK_PIN, SPEAKER_LRCLK_PIN, SPEAKER_DATA_PIN);
  audio.setVolume(17);
  s_audio_ready = true;
  Serial.printf("Audio init: BCLK=%d LRCLK=%d DOUT=%d\r\n",
                SPEAKER_BCLK_PIN, SPEAKER_LRCLK_PIN, SPEAKER_DATA_PIN);
}

extern "C" void gui_hal_audio_loop(void) {
  if (!s_audio_ready) return;
  audio.loop();
  // Deferred amplifier enable keeps startup/track-switch transients out of the speaker.
  if (s_amp_enable_at_ms != 0 && (int32_t)(millis() - s_amp_enable_at_ms) >= 0) {
    s_amp_enable_at_ms = 0;
    board_set_speaker_amp(true);
  }
}

extern "C" bool gui_hal_get_artist_album_by_index(size_t index,
                                                  char *artist,
                                                  size_t artist_len,
                                                  char *album,
                                                  size_t album_len) {
  (void)index;
  if (artist && artist_len > 0) strlcpy(artist, "Unknown artist", artist_len);
  if (album && album_len > 0) strlcpy(album, "Unknown", album_len);
  return false;
}
