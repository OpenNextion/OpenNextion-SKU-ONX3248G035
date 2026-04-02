/**
 * @file board_speaker.c
 * @brief Hardware Driver for NS4168 Speaker Amplifier (ESP32 I2S Interface)
 * @details Implements speaker subsystem initialization, power control (on/off), and audio playback
 *          via ESP32 standard I2S driver. Uses PCF8574 I2C GPIO expander to control the amplifier enable pin,
 *          and configures I2S master TX channel for 16-bit stereo audio output to NS4168 codec.
 * @note Critical Dependencies:
 *       - driver/i2s_std.h: ESP32 standard I2S peripheral driver
 *       - exio_pcf8574.h: PCF8574 I2C GPIO expander driver (I2S_CTRL pin control)
 *       - board_speaker.h: Hardware pin definitions (BCLK/WS/SDATA for NS4168)
 *       - I2S_FRE: Predefined I2S sample rate (e.g., 44100/48000 Hz)
 * @version 1.0
 * @date 2025
 */

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "exio_pcf8574.h"
#include "board_speaker.h"
#include "exio_pcf8574.h"

/* -------------------------------------------------------------------------- */
/* Static Module Variables (Private)                                          */
/* -------------------------------------------------------------------------- */

/** @brief I2S transmit channel handle (master TX role for audio output to NS4168) */
static i2s_chan_handle_t tx_handle;

/** @brief Speaker power state flag (true = amplifier/I2S enabled, false = disabled) */
static bool  Speaker_state = false;

/* -------------------------------------------------------------------------- */
/* Public Speaker Driver Functions (API)                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize speaker subsystem (I2S + amplifier control)
 * @details Performs full speaker hardware initialization in two key steps:
 *          1. Enable NS4168 amplifier via PCF8574 (set I2S_CTRL pin to HIGH)
 *          2. Configure and enable ESP32 I2S master TX channel:
 *             - Uses default clock config for predefined I2S_FRE sample rate
 *             - Configures 16-bit stereo slot mode (MSB first)
 *             - Maps BCLK/WS/SDATA GPIOs to NS4168 pins (din/mclk unused)
 * @note Fatal error handling: Uses ESP_ERROR_CHECK to abort execution if any step fails
 *       (I2S initialization failure is unrecoverable for audio playback)
 * @return None (void function, errors trigger system abort via ESP_ERROR_CHECK)
 */
void speaker_Init(void)
{
    // Enable NS4168 amplifier by setting PCF8574 I2S_CTRL pin high
    ESP_ERROR_CHECK (exio_pcf8574_pin_write(EXIO_PCF8574_PIN_I2S_CTRL,EXIO_PCF8574_LEVEL_H));

    // Configure I2S TX channel (master role, predefined I2S_NUM port)
    i2s_chan_config_t tx_handle_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM,I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_handle_cfg,&tx_handle,NULL));
    
    // Initialize I2S standard mode parameters for NS4168
    i2s_std_config_t tx_std_cfg = {
            .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_FRE),  // Clock config for target sample rate (I2S_FRE)
            .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),  // 16-bit stereo
            .gpio_cfg = {
                    .mclk = -1,    // MCLK not required for NS4168 codec
                    .bclk = SPEAKER_NS4168_BCLK,  // I2S bit clock pin (BCLK)
                    .ws   = SPEAKER_NS4168_WS_LRCLK,  // I2S word select (LRCLK)
                    .dout = SPEAKER_NS4168_SDATA,  // I2S data output (SDATA)
                    .din  = -1,    // No input (TX-only for speaker)
                    .invert_flags = {  // No pin inversion required
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv   = false,
                    },
            },
    };
    // Apply I2S standard mode configuration
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &tx_std_cfg));
    // Enable I2S TX channel (ready for audio data transmission)
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    // Mark speaker as initialized/enabled
    Speaker_state = true;
}

/**
 * @brief Turn on speaker (enable amplifier/I2S subsystem)
 * @details Enables the speaker only if two preconditions are met:
 *          1. I2S TX channel is initialized (tx_handle != NULL)
 *          2. Speaker is currently off (Speaker_state = false)
 *          - Sets PCF8574 I2S_CTRL pin HIGH to enable NS4168 amplifier
 *          - Updates Speaker_state flag to reflect active state
 * @note No action if speaker is already on or I2S is uninitialized (prevents redundant writes)
 * @return None
 */
void ctrl_speaker_on(void)
{
    if((tx_handle != NULL) && (Speaker_state == false))
    {
        Speaker_state =   true;  
        // Enable amplifier via PCF8574 I2S_CTRL pin
        ESP_ERROR_CHECK (exio_pcf8574_pin_write(EXIO_PCF8574_PIN_I2S_CTRL,EXIO_PCF8574_LEVEL_H));
        printf("\r\n\r\n **** ctrl_speaker_on **** \r\n\r\n");
    }
}

/**
 * @brief Turn off speaker (disable amplifier/I2S subsystem)
 * @details Disables the speaker only if two preconditions are met:
 *          1. I2S TX channel is initialized (tx_handle != NULL)
 *          2. Speaker is currently on (Speaker_state = true)
 *          - Sets PCF8574 I2S_CTRL pin LOW to disable NS4168 amplifier
 *          - Updates Speaker_state flag to reflect inactive state
 * @note No action if speaker is already off or I2S is uninitialized (prevents redundant writes)
 * @return None
 */
void ctrl_speaker_off(void)
{
    if((tx_handle != NULL) && (Speaker_state == true))
    {
        // Disable amplifier via PCF8574 I2S_CTRL pin
        ESP_ERROR_CHECK (exio_pcf8574_pin_write(EXIO_PCF8574_PIN_I2S_CTRL,EXIO_PCF8574_LEVEL_L));
        Speaker_state =   false;  
        printf("\r\n\r\n **** ctrl_speaker_off **** \r\n\r\n");
    }
}

/**
 * @brief Transmit audio data to speaker via I2S
 * @details Writes PCM audio buffer to NS4168 amplifier through ESP32 I2S TX channel.
 *          Performs strict input validation before attempting I2S write:
 *          - Audio buffer pointer is not NULL
 *          - Size-written output pointer is not NULL
 *          - I2S TX channel is initialized (tx_handle != NULL)
 *          - Speaker is in active state (Speaker_state = true)
 * @param play_buf Pointer to PCM audio buffer (16-bit stereo samples, byte-aligned)
 * @param play_size Size of audio buffer in bytes (must be multiple of 2 for 16-bit samples)
 * @param size_written Output pointer: number of bytes actually transmitted to I2S (set by i2s_channel_write)
 * @param time Timeout in milliseconds for I2S write operation (0 = non-blocking, portMAX_DELAY = blocking)
 * @return esp_err_t: ESP_OK = successful write, ESP_FAIL = input validation failed (no write attempted)
 * @note Returns ESP_FAIL immediately if any precondition fails (avoids invalid I2S operations)
 */
esp_err_t speaker_play(const void * play_buf,size_t play_size,size_t *size_written,uint32_t time)
{
    // Validate input parameters and speaker state before I2S write
    if((play_buf != NULL) && (size_written != NULL) && (tx_handle != NULL) && (Speaker_state == true))
    {
        // Forward audio buffer to I2S TX channel (returns I2S write result)
        return i2s_channel_write(tx_handle, play_buf, play_size, size_written, time);
    }
    // Input validation failed: return error
    return ESP_FAIL;
}