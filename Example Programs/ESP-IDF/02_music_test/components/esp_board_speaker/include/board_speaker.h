/**
 * @file board_speaker.h
 * @brief Public API Header for NS4168 Speaker Amplifier Driver (ESP32 Platform)
 * @details Defines hardware-specific constants (GPIO pins, I2S config) and public function prototypes
 *          for controlling the NS4168 audio amplifier via ESP32 I2S peripheral.
 *          This header exposes the speaker driver's public interface to upper-layer modules (e.g., GUI HAL).
 * @note Compatibility: Wrapped in extern "C" to support both C and C++ compilation
 * @dependencies:
 *  - esp_err.h: ESP32 standard error code definitions (for esp_err_t return type)
 *  - driver/gpio.h: ESP32 GPIO number definitions (implicit dependency for GPIO_NUM_x)
 *  - driver/i2s_std.h: ESP32 I2S peripheral definitions (implicit dependency for I2S_NUM_x)
 * @version 1.0
 * @date 2025
 */
#pragma once

#include "esp_err.h"

// Ensure C linkage for C++ compilation (prevents name mangling)
#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Hardware Pin & I2S Configuration Constants (Public)                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief GPIO pin number for NS4168 I2S Bit Clock (BCLK) signal
 * @note Connected to NS4168's BCLK pin (I2S clock synchronization)
 */
#define SPEAKER_NS4168_BCLK       GPIO_NUM_14

/**
 * @brief GPIO pin number for NS4168 I2S Serial Data (SDATA) signal
 * @note Connected to NS4168's DIN pin (PCM audio data input to amplifier)
 */
#define SPEAKER_NS4168_SDATA      GPIO_NUM_15    

/**
 * @brief GPIO pin number for NS4168 I2S Word Select (WS/LRCLK) signal
 * @note Connected to NS4168's LRCLK pin (left/right channel selection for stereo audio)
 */
#define SPEAKER_NS4168_WS_LRCLK   GPIO_NUM_16 

/**
 * @brief Default I2S sample rate (Hz) for audio playback
 * @note Set to 44100 Hz (standard audio CD quality) for NS4168 amplifier
 */
#define I2S_FRE     44100   

/**
 * @brief ESP32 I2S peripheral port number used for speaker output
 * @note Uses I2S_NUM_1 (second I2S controller) to avoid conflict with other peripherals
 */
#define I2S_NUM     I2S_NUM_1

/* -------------------------------------------------------------------------- */
/* Public Speaker Driver API Function Prototypes                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize speaker subsystem (I2S peripheral + NS4168 amplifier)
 * @details Configures ESP32 I2S master TX channel for 16-bit stereo audio and enables
 *          the NS4168 amplifier via PCF8574 GPIO expander. Must be called once before
 *          any other speaker functions (e.g., ctrl_speaker_on/off, speaker_play).
 * @note Fatal error: Aborts system execution if initialization fails (via ESP_ERROR_CHECK)
 * @return None (void function, no return value)
 */
void speaker_Init(void);

/**
 * @brief Turn off the speaker (disable NS4168 amplifier)
 * @details Disables the NS4168 amplifier by setting the PCF8574 I2S_CTRL pin to LOW,
 *          and updates the internal speaker state flag. No action if speaker is already off
 *          or I2S peripheral is uninitialized.
 * @return None (void function, no return value)
 */
void ctrl_speaker_off(void);

/**
 * @brief Turn on the speaker (enable NS4168 amplifier)
 * @details Enables the NS4168 amplifier by setting the PCF8574 I2S_CTRL pin to HIGH,
 *          and updates the internal speaker state flag. No action if speaker is already on
 *          or I2S peripheral is uninitialized.
 * @return None (void function, no return value)
 */
void ctrl_speaker_on(void);

/**
 * @brief Transmit PCM audio data to the speaker via I2S
 * @details Writes a buffer of 16-bit stereo PCM audio data to the ESP32 I2S TX channel,
 *          which is sent to the NS4168 amplifier. Performs input validation to prevent
 *          invalid I2S operations (e.g., null pointers, speaker off state).
 * @param play_buf Pointer to PCM audio buffer (16-bit signed integers, stereo format)
 * @param play_size Size of the audio buffer in bytes (must be multiple of 2 for 16-bit samples)
 * @param size_written Output pointer: number of bytes actually transmitted to I2S (set by driver)
 * @param time Timeout in milliseconds for the I2S write operation (0 = non-blocking, portMAX_DELAY = blocking)
 * @return esp_err_t: ESP_OK = successful write, ESP_FAIL = input validation failed (no write attempted)
 */
esp_err_t speaker_play(const void * play_buf,size_t play_size,size_t *size_written,size_t time);



/* Helper APIs moved down to board_speaker.c (optional refactor).
 * For clean builds, add these declarations to board_speaker.h instead.
 */
void speaker_set_pcm_format(uint32_t rate_hz, uint32_t bits_cfg, uint8_t channels);
void speaker_flush_silence_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif