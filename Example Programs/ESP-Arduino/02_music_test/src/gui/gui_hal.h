#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"

#include <stddef.h>
#include <stdbool.h>

#define MAX_MP3_NUM (50)

/**
 * @brief Scan the SD-card music directory and initialize the file iterator.
 *
 * All files in "/sdcard/music" are currently counted, but only ".mp3" files
 * will be played by the audio HAL.
 */
esp_err_t gui_hal_music_scan(void);

/**
 * @brief Get the number of tracks discovered by the last scan.
 *
 * This is typically the number of entries in the iterator, clipped by
 * MAX_MP3_NUM.
 *
 * @return Track count.
 */
size_t gui_hal_get_track_count(void);

/**
 * @brief Get the display title of a track by index.
 *
 * The title is derived from the file name (base name without extension).
 *
 * @param index   Track index in [0, gui_hal_get_track_count()).
 * @param buf     Output buffer for the title string.
 * @param buf_len Size of the output buffer in bytes.
 * @return true on success, false on error or out-of-range index.
 */
bool gui_hal_get_title_by_index(size_t index, char *buf, size_t buf_len);

/**
 * @brief Get an estimated length of a track in seconds.
 *
 * The length is estimated from the file size and a fixed bitrate and is only
 * intended for UI purposes (e.g. slider range), not for precise timing.
 *
 * @param index Track index in [0, gui_hal_get_track_count()).
 * @return Estimated track length in seconds, or 0 on error.
 */
uint32_t gui_hal_get_track_length(size_t index);

/**
 * @brief Play the track at the given index.
 *
 * This function keeps the iterator index in sync and starts playback of the
 * given track regardless of the previous playback state.
 *
 * @param index Track index in [0, gui_hal_get_track_count()).
 */
void gui_hal_play_index(size_t index);

/**
 * @brief Fetch a pending "track end / auto-next" event from the audio callback.
 *
 * Called from the GUI task to synchronize UI with the audio engine when it
 * automatically advances to the next track.
 *
 * @param p_index Pointer to receive the current track index.
 * @return true if an index was received, false if the queue is empty.
 */
bool gui_hal_fetch_track_end(uint32_t *p_index);

/**
 * @brief Set software volume in the range [0, 100].
 */
void gui_hal_set_volume(uint8_t vol);

/**
 * @brief Get current software volume in the range [0, 100].
 */
uint8_t gui_hal_get_volume(void);

/**
 * @brief Toggle play/pause state.
 *
 * When idle, this starts playback of the current iterator index.
 * When paused, this resumes playback.
 * When playing, this pauses playback.
 */
void gui_hal_toggle_play_pause(void);

/**
 * @brief Switch to previous or next track.
 *
 * @param is_next Non-zero for next track, 0 for previous track.
 *
 * If the audio player is playing, the new track is started immediately.
 * If paused, the new track is loaded and then paused again.
 * If idle, only the iterator index is moved and playback is not started.
 */
void gui_hal_prev_next(uint8_t is_next);

/**
 * @brief Initialize the audio player and related HAL state.
 */
void gui_hal_audio_init(void);

/**
 * @brief Keep the Arduino audio decoder/I2S output running.
 *
 * The ESP32-audioI2S library requires frequent loop calls from the Arduino
 * main loop. This function is an Arduino-only extension used by the sketch.
 */
void gui_hal_audio_loop(void);



bool gui_hal_get_artist_album_by_index(size_t index,
                                      char *artist, size_t artist_len,
                                      char *album,  size_t album_len);

#ifdef __cplusplus
}
#endif
