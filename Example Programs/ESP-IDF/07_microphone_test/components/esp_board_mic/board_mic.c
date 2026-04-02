/**
 * @file board_mic.c
 * @brief PDM Microphone Driver for ESP32 (I2S PDM RX Mode)
 * @details Implements initialization and data read functions for a PDM (Pulse Density Modulation)
 *          microphone using ESP32's I2S peripheral in PDM RX mode. Supports 16-bit stereo audio
 *          sampling at a configurable sample rate (RECORD_SAMPLE_RATE).
 * @note Dependencies:
 *       - driver/i2s_std.h: ESP32 standard I2S driver definitions
 *       - driver/i2s_pdm.h: ESP32 PDM I2S mode driver definitions
 *       - board_mic.h: Board-specific PDM mic pin definitions (PDM_CLK_IO, PDM_DATA_IO) and constants
 *       - RECORD_SAMPLE_RATE: Predefined sampling rate for microphone recording (e.g., 16000/44100 Hz)
 * @version 1.0
 * @date 2025
 */

#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"
#include "board_mic.h"

/* Static log tag for ESP_LOGx macro output (module identification) */
static const char *TAG = "BOARD_MIC";

/** @brief I2S channel handle for PDM microphone RX (receive) operations */
static i2s_chan_handle_t pdm_rx_chan = NULL;

/**
 * @brief Initialize PDM microphone subsystem (I2S PDM RX mode)
 * @details Configures ESP32 I2S peripheral (I2S_NUM_0) as master to communicate with PDM microphone:
 *          1. Creates I2S RX channel (master role)
 *          2. Initializes PDM RX mode with clock/slot/GPIO configurations
 *          3. Enables the I2S channel (ready for audio data reception)
 * @note Fatal error handling: Uses ESP_ERROR_CHECK to abort execution if any step fails
 *       (PDM mic initialization failure is unrecoverable for audio recording)
 * @return esp_err_t: ESP_OK on successful initialization (fatal errors trigger system abort)
 */
esp_err_t init_pdm_microphone(void) 
{
    // Configure I2S RX channel (master role, use I2S_NUM_0 peripheral)
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    // Create new I2S channel (RX only, TX handle set to NULL)
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &pdm_rx_chan));

    // Configure PDM RX mode parameters for microphone
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        // Default clock configuration for target recording sample rate (RECORD_SAMPLE_RATE)
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(RECORD_SAMPLE_RATE),
        // Slot configuration: 16-bit data width, stereo mode (compatible with most PDM mics)
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, 
                                                   I2S_SLOT_MODE_STEREO),
        // GPIO pin mapping for PDM microphone (board-specific definitions)
        .gpio_cfg = {
            .clk = PDM_CLK_IO,       // PDM clock output pin (driven by ESP32 master)
            .din = PDM_DATA_IO,      // PDM data input pin (from microphone)
            .invert_flags = {
                .clk_inv = false,    // Do not invert PDM clock signal (default for most mics)
            },
        }
    };
    
    // Initialize I2S channel in PDM RX mode with above configuration
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(pdm_rx_chan, &pdm_rx_cfg));
    // Enable the I2S RX channel (ready to receive PDM audio data)
    ESP_ERROR_CHECK(i2s_channel_enable(pdm_rx_chan));
    
    return ESP_OK;
}

/**
 * @brief Read audio data from PDM microphone via I2S
 * @details Wraps ESP32 I2S channel read function to retrieve raw PDM audio data from microphone.
 *          Performs direct read from the initialized I2S PDM RX channel without additional processing.
 * @param read_buf Pointer to buffer to store received audio data (16-bit stereo samples)
 * @param will_read_len Number of bytes to read (must be multiple of 2 for 16-bit samples)
 * @param real_read_data Output pointer: number of bytes actually read (set by I2S driver)
 * @param time Timeout in milliseconds for read operation (0 = non-blocking, portMAX_DELAY = blocking)
 * @return esp_err_t: ESP_OK on successful read, ESP_ERR_* on failure (e.g., timeout, invalid channel)
 */
esp_err_t mic_read(void * read_buf,int will_read_len, size_t *real_read_data,uint32_t time) 
{
    // Read raw audio data from PDM RX channel and return operation status
    esp_err_t err  =  i2s_channel_read(pdm_rx_chan, read_buf, will_read_len, real_read_data, time);
    return err;
}