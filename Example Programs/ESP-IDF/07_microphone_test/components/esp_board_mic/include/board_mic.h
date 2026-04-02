#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Recording sample rate in Hz.
 *
 * Must match the PDM microphone / AFE configuration.
 */
#define RECORD_SAMPLE_RATE  16000

/**
 * @brief Recording bit depth.
 */
#define RECORD_BIT_DEPTH    16

/**
 * @brief Number of input channels from the microphone front-end.
 *
 * Typically 1 for mono or 2 for stereo/dual-mic.
 */
#define RECORD_CHANNELS     2

/**
 * @brief PDM microphone clock GPIO.
 */
#define PDM_CLK_IO          GPIO_NUM_19

/**
 * @brief PDM microphone data GPIO.
 */
#define PDM_DATA_IO         GPIO_NUM_20

/**
 * @brief Initialize PDM microphone front-end (I2S / PDM peripheral).
 *
 * This function should configure the I2S peripheral and any related
 * GPIOs required for the PDM microphone.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t init_pdm_microphone(void);

/**
 * @brief Read PCM data from the microphone.
 *
 * This function reads decoded PCM samples (after PDM / I2S front-end)
 * into the provided buffer.
 *
 * @param read_buf       Pointer to destination buffer.
 * @param will_read_len  Number of bytes to read.
 * @param real_read_data Pointer to variable that receives the number
 *                       of bytes actually read.
 * @param time           Timeout in RTOS ticks or milliseconds
 *                       (depending on implementation).
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t mic_read(void *read_buf,
                   int will_read_len,
                   size_t *real_read_data,
                   uint32_t time);

#ifdef __cplusplus
}
#endif
