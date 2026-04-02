#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "driver/i2s_std.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "esp_lcd_touch_cst826.h"
#include "esp_lcd_st7796u.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "exio_pcf8574.h"
#include "board_mic.h"
#include "board_speaker.h"

#include "model_path.h"
#include "esp_dsp.h"
#include "esp_afe_sr_models.h"

LV_FONT_DECLARE(ui_mic_font);

#define MY_MIC_SYMBOL "\xEE\x9A\x83"

// LCD / touch / backlight configuration
#define NEXTION_IIC_SCL_PIN  7
#define NEXTION_IIC_SDA_PIN  8

#define LCD_H_RES    (320)
#define LCD_V_RES    (480)

#define I2C_PORT_NUM           I2C_NUM_0
#define LCD_HOST               SPI2_HOST

#define LCD_SCLK_PIN           GPIO_NUM_5
#define LCD_MOSI_PIN           GPIO_NUM_1
#define LCD_RST_PIN            GPIO_NUM_NC
#define LCD_PWR_PIN            GPIO_NUM_NC
#define LCD_DC_PIN             GPIO_NUM_3
#define LCD_CS_PIN             GPIO_NUM_2

#define LCD_BL_PIN             GPIO_NUM_6
#define LCD_BL_LEDC_TIMER      LEDC_TIMER_1
#define LCD_BL_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LCD_BL_LEDC_CHANNEL    LEDC_CHANNEL_0
#define LCD_BL_LEDC_DUTY_RES   LEDC_TIMER_10_BIT
#define LCD_BL_LEDC_DUTY       (1024)
#define LCD_BL_LEDC_FREQUENCY  (10000)

// Maximum recording duration in seconds
#define RECORD_SECONDS  30

static const char *TAG = "MICROPHONE_TEST";

static lv_obj_t *tite_label = NULL;
static lv_obj_t *bnt_mic = NULL;
static lv_obj_t *bnt_stop = NULL;

// AFE handles
static const esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;

// Event group for audio control flags
static EventGroupHandle_t audio_event = NULL;
#define AUDIO_EVENT_PLAY    (0x01 << 0)   // Playback event
#define AUDIO_EVENT_RECORD  (0x01 << 1)   // Record event
#define AUDIO_EVENT_SAVE    (0x01 << 2)   // Reserved

// Mutex to serialize record / playback
static SemaphoreHandle_t play_record_mutex = NULL;

// I2S / audio processing buffer configuration
#define DMA_BUF_LEN  1024



// In-memory recording buffer (AFE output: 16 kHz / 16-bit / mono PCM)
static int16_t *s_record_buffer   = NULL;   // PCM buffer
static size_t   s_record_bytes    = 0;      // Used bytes in buffer
static size_t   s_record_capacity = 0;      // Total capacity in bytes
static bool     s_record_ready    = false;  // True when a valid recording is available

// I2C / LCD static handles
static i2c_master_bus_handle_t   esp32_i2c_bus_num0_handle = NULL;
static esp_lcd_touch_handle_t    touch_handle = NULL;
static esp_lcd_panel_handle_t    panel_handle = NULL;
static esp_lcd_panel_io_handle_t lcd_io_handle = NULL;


/* ----------------- Board-level I2C / LCD / backlight / LVGL initialization ----------------- */

// LVGL display device handle (stores the registered display in LVGL)
static lv_display_t *lvgl_disp = NULL;

/**
 * @brief Initialize the I2C master bus (I2C_NUM_0) for board-level communication
 * 
 * This function configures and initializes the I2C master bus (port 0) with specified 
 * GPIO pins for SCL (Serial Clock Line) and SDA (Serial Data Line), sets up clock source,
 * and enables internal pull-up resistors. It creates the I2C master bus handle for 
 */
void board_iic_init(void)
{
    // Configuration structure for I2C master bus parameters
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,         // Use default I2C clock source (typically APB clock)
        .i2c_port = I2C_PORT_NUM,                  // I2C port number (0 in this case)
        .scl_io_num = NEXTION_IIC_SCL_PIN,         // GPIO pin number for SCL (connected to Nextion device)
        .sda_io_num = NEXTION_IIC_SDA_PIN,         // GPIO pin number for SDA (connected to Nextion device)
        .glitch_ignore_cnt = 7,                    // Number of glitch pulses to ignore on the bus (noise filtering)
        .flags.enable_internal_pullup = true       // Enable internal pull-up resistors for SCL/SDA lines
    };

    // Create and initialize the I2C master bus with the above configuration,
    // and store the bus handle in esp32_i2c_bus_num0_handle.
    // ESP_ERROR_CHECK aborts if initialization fails (e.g., invalid pins or port).
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &esp32_i2c_bus_num0_handle));
}


/**
 * @brief Initialize the CST826 touch screen controller via I2C
 * 
 * This function sets up the I2C communication bus, configures the touch screen IO parameters,
 * and initializes the CST826 touch controller with specified display dimensions and coordinate settings.
 * It establishes the necessary handles for I2C bus communication and touch screen operation.
 */
void lcd_cst826_touch_Init(void)
{
    // Handle for I2C master bus (bus 0)
    i2c_master_bus_handle_t NUM_0_handle;
    // Handle for touch screen I2C IO communication
    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    
    // Configuration structure for I2C communication with the touch screen
    esp_lcd_panel_io_i2c_config_t touch_io_config = {};
    touch_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST826_ADDRESS;  // I2C device address of CST826
    touch_io_config.control_phase_bytes = 1;                         // Number of control phase bytes
    touch_io_config.dc_bit_offset = 0;                               // Offset of DC bit in control phase
    touch_io_config.lcd_cmd_bits = 8;                                // Number of bits for LCD commands
    touch_io_config.flags.disable_control_phase = 1;                  // Disable control phase for touch communication
    touch_io_config.scl_speed_hz = 400 * 1000;                       // I2C SCL clock frequency (400 KHz)

    // Get the handle for I2C master bus 0
    ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &NUM_0_handle));

    // Create I2C IO handle for touch screen if the I2C bus handle is valid
    if(NUM_0_handle != NULL)
    {
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(NUM_0_handle, &touch_io_config, &touch_io_handle));
    }

    // Configuration structure for touch screen parameters
    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = LCD_H_RES;                         // Maximum X coordinate of the display
    tp_cfg.y_max = LCD_V_RES;                         // Maximum Y coordinate of the display
    tp_cfg.rst_gpio_num = GPIO_NUM_NC;                // Reset GPIO pin (not used, NC = No Connect)
    tp_cfg.int_gpio_num = GPIO_NUM_NC;                // Interrupt GPIO pin (not used)

    tp_cfg.flags.swap_xy = 0;                         // Do not swap X and Y coordinates
    tp_cfg.flags.mirror_x = 0;                        // Do not mirror X coordinate
    tp_cfg.flags.mirror_y = 0;                        // Do not mirror Y coordinate

    // Initialize CST826 touch controller with the configured parameters and get the touch handle
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst826(touch_io_handle, &tp_cfg, &touch_handle));
}



/**
 * @brief  Fill the entire LCD screen with a specified 16-bit RGB565 color
 * @param  color  Target fill color in RGB565 format (uint16_t type, e.g., 0xF800 for pure red)
 * @note   This function adopts a dual-strategy filling mechanism:
 *         1. Priority: Use external PSRAM to allocate a batch buffer for high-efficiency filling
 *         2. Fallback: Use stack-based single-row buffer if PSRAM allocation fails (low memory footprint but slower)
 * @warning Global variable `panel_handle` (LCD panel handle) must be initialized before calling this function
 */
void lcd_fill_screen(uint16_t color)
{
    // Screen horizontal resolution (width)
    const int W = LCD_H_RES;
    // Screen vertical resolution (height)
    const int H = LCD_V_RES;
    // Number of rows processed per batch (split screen height into 4 parts for optimized memory/transmission)
    const int LINES = LCD_V_RES/4;

    // Total pixels in one batch (width * lines per batch)
    int pixels = W * LINES;
    // Allocate batch buffer from external PSRAM (high capacity, suitable for large data transmission)
    uint16_t *buf = heap_caps_malloc(pixels * sizeof(uint16_t),
                                     MALLOC_CAP_SPIRAM );

    // Case 1: PSRAM buffer allocation failed (use stack buffer fallback)
    if(buf == NULL)
    {   
        ESP_LOGE(TAG, "PSRAM buffer allocation failed, using stack buffer fallback");
        // Stack buffer for single row of pixels (avoids heap memory fragmentation, small footprint)
        uint16_t buffer[LCD_H_RES];
        // Initialize single-row buffer: fill all pixel positions with target color
        for (uint32_t i = 0; i < LCD_H_RES ; i++)
        {
            buffer[i] = (color); // Assign target RGB565 color to each pixel in the row buffer
        }

        // Traverse all rows of the LCD and draw row by row
        for (uint16_t y = 0; y < LCD_V_RES; y++) // Note: Fixed original "y <= LCD_V_RES" to avoid out-of-bounds
        {
            // Draw current row: x range [0, W), y range [y, y+1) (single row height)
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y+1, buffer);
        }
    }
    // Case 2: PSRAM buffer allocation succeeded (high-efficiency batch filling)
    else
    {
        ESP_LOGI(TAG, "PSRAM buffer allocation succeeded, using batch filling"); // Fixed original "LOGE" to "LOGI" (log level correction)
        // Fill the PSRAM batch buffer with target color (preload all pixels in one batch)
        for(int i = 0; i < pixels; i++) 
        {
            buf[i] = color;
        }
        // Traverse the screen in batches (process LINES rows each time)
        for(int y = 0; y < H; y += LINES) 
        {
            // Calculate actual batch height (handle the last batch to avoid screen boundary overflow)
            int h = (y + LINES <= H) ? LINES : (H - y);
            // Draw batch pixels: x range [0, W), y range [y, y+h)
            esp_lcd_panel_draw_bitmap(panel_handle, 0, y, W, y + h, buf);
        }
        // Free the PSRAM buffer to avoid memory leakage (must use heap_caps_free for PSRAM allocation)
        heap_caps_free(buf);
    }   
}


/**
 * @brief Initialize ST7796U LCD panel via SPI interface
 * 
 * This function performs full initialization of the ST7796U LCD panel, including hardware reset via PCF8574 expander,
 * SPI bus configuration, LCD SPI IO setup, panel parameter configuration, and final activation of the display.
 * It establishes the necessary handles for SPI bus, LCD IO, and panel operation for subsequent display operations.
 * 
 * @return esp_err_t ESP_OK if initialization succeeds, error code otherwise
 */
esp_err_t Lcd_Init(void)
{
    // Configuration structure for SPI bus parameters
    spi_bus_config_t buscfg = {
        .sclk_io_num     = LCD_SCLK_PIN,    /* SPI SCLK (clock) pin number */
        .mosi_io_num     = LCD_MOSI_PIN,    /* SPI MOSI (master out, slave in) pin number */
        .miso_io_num     = -1,              /* SPI MISO (master in, slave out) pin - not used (-1 = disabled) */
        .quadwp_io_num   = -1,              /* WP pin for SPI Quad mode - not used (-1 = disabled) */
        .quadhd_io_num   = -1,              /* HD pin for SPI Quad mode - not used (-1 = disabled) */
        .max_transfer_sz = LCD_H_RES * LCD_V_RES / 8   /* Maximum SPI transfer size in bytes */
    };
    /* Initialize SPI bus with the configured parameters */
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* Configuration structure for LCD SPI IO interface */
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num         = LCD_DC_PIN,          /* GPIO pin for DC (Data/Command) control */
        .cs_gpio_num         = LCD_CS_PIN,          /* GPIO pin for CS (Chip Select) control */
        .pclk_hz             = 80 * 1000 * 1000,    /* SPI clock frequency (80 MHz) */
        .lcd_cmd_bits        = 8,                   /* Number of bits for LCD command transmission */
        .lcd_param_bits      = 8,                   /* Number of bits for LCD parameter/data transmission */
        .spi_mode            = 0,                   /* SPI mode (mode 0: CPOL=0, CPHA=0) */
        .trans_queue_depth   = 5,                   /* Depth of the SPI transfer queue */
        .on_color_trans_done = NULL,                /* Callback function for color transfer completion - not used */
        .user_ctx = NULL,                           /* User context pointer for callback - not used */
    };
    /* Attach LCD panel to the initialized SPI bus and create LCD IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io_handle));

    /* Configuration structure for LCD panel device parameters */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,                  /* LCD reset GPIO pin number */
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,    /* BGR color element order */
        .bits_per_pixel = 16,                           /* Color depth (16 bits per pixel, RGB565) */
    };


    /* Create ST7796U LCD panel handle and bind to the SPI IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796u(lcd_io_handle, &panel_config, &panel_handle));

    /* Perform software reset on the LCD panel via panel handle */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    // Supplementary hardware reset via PCF8574 expander
    exio_pcf8574_pin_write(EXIO_PCF8574_PIN_LCD_RST, EXIO_PCF8574_LEVEL_L);
    vTaskDelay(pdMS_TO_TICKS(100));   // Delay 100ms for reset
    exio_pcf8574_pin_write(EXIO_PCF8574_PIN_LCD_RST, EXIO_PCF8574_LEVEL_H);
    vTaskDelay(pdMS_TO_TICKS(100));   // Delay 100ms for stabilization


    /* Initialize the LCD panel with configured parameters */
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    /* Configure LCD color inversion (false = disable inversion) */
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    /* Turn on the LCD panel (enable display output) */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
     // Fill screen with black (RGB565: 0x0000) after initialization
    lcd_fill_screen(0x0000);

    vTaskDelay(pdMS_TO_TICKS(50));
    
    return ESP_OK;   // Return success status
}



/**
 * @brief Initialize LCD brightness (BL) control via ESP32 LEDC PWM
 * 
 * This function configures the ESP32 LEDC (LED Controller) peripheral to generate PWM signals,
 * which are used to control the LCD brightness brightness. It initializes the LEDC timer (for PWM frequency)
 * and LEDC channel (for PWM output to the brightness GPIO pin), with initial duty cycle set to 0% (brightness off).
 */
void lcd_brightness_init(void)
{
    // Configure LEDC timer parameters for PWM signal generation
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LCD_BL_LEDC_MODE;       /* LEDC speed mode (high-speed/low-speed) for brightness */
    ledc_timer.timer_num = LCD_BL_LEDC_TIMER;       /* LEDC timer number assigned to brightness control */
    ledc_timer.duty_resolution = LCD_BL_LEDC_DUTY_RES; /* PWM duty cycle resolution (bits, e.g., 8-bit = 0-255) */
    ledc_timer.freq_hz = LCD_BL_LEDC_FREQUENCY;     /* PWM output frequency (configured to 5 kHz here) */
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;             /* Automatically select LEDC clock source */
    // Apply the LEDC timer configuration (abort if configuration fails)
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Configure LEDC channel parameters for PWM output
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = LCD_BL_LEDC_MODE;     /* Match the speed mode of the configured timer */
    ledc_channel.channel = LCD_BL_LEDC_CHANNEL;     /* LEDC channel number assigned to brightness */
    ledc_channel.timer_sel = LCD_BL_LEDC_TIMER;     /* Bind the channel to the previously configured timer */
    ledc_channel.intr_type = LEDC_INTR_DISABLE;     /* Disable interrupt for this LEDC channel */
    ledc_channel.gpio_num = LCD_BL_PIN;             /* GPIO pin connected to LCD brightness control */
    ledc_channel.duty = 0;                          /* Initial PWM duty cycle (0% = brightness off initially) */
    ledc_channel.hpoint = 0;                        /* LEDC hpoint value (0 = default, no phase shift) */
    // Apply the LEDC channel configuration (abort if configuration fails)
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}




/**
 * @brief Sets the brightness of the LCD screen
 * @param intput_brightness Input brightness value (expected range: 0-100, representing percentage)
 *                          Note: There is a typo in the parameter name, should be "input_brightness"
 */
void lcd_brightness_set(uint8_t intput_brightness)
{
    // Check if input brightness exceeds the valid range (0-100)
    if (intput_brightness > 100)
    {
        // Clamp the brightness to the maximum valid value (100) if out of range
        intput_brightness = 100;
        // Log an error message indicating the input value is out of range
        ESP_LOGE(TAG, "intput_brightness value out of range");
    }

    // Calculate the duty cycle for the LED controller
    // Principle: Convert 0-100 brightness percentage to the duty cycle range supported by the LED controller
    // LCD_BL_LEDC_DUTY should be the maximum duty cycle of the LED controller 
    uint32_t duty = (intput_brightness * (LCD_BL_LEDC_DUTY - 1)) / 100;  

    // Set the duty cycle for the specified LED controller channel (LCD_BL_LEDC_CHANNEL is the backlight control channel)
    // ESP_ERROR_CHECK is used to check the function return value; triggers error handling if failed
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL, duty));
    // Update the LED controller's duty cycle to make the setting take effect
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL));

    // Log an info message to record the result of the brightness setting
    ESP_LOGI(TAG, "LCD brightness set to %d%%", intput_brightness);
}



/**
 * @brief Initialize LVGL port (display + touch) for ESP32
 * 
 * This function initializes the LVGL port with default configuration, configures display parameters 
 * (resolution, buffer, rotation, memory flags, etc.), adds the LCD panel as an LVGL display device,
 * and binds the touch controller as an LVGL input device. It bridges the hardware (LCD/touch) with 
 * LVGL's software framework to enable UI rendering and touch interaction.
 */
void lv_port_init(void)
{
    // LVGL port core configuration, initialized with default settings via macro
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // Initialize LVGL port with the default configuration
    lvgl_port_init(&port_cfg);
    ESP_LOGI(TAG, "lv_port_init");  // Log successful LVGL port core initialization

    // LVGL touch input device handle (stores the registered touch in LVGL)
    static lv_indev_t *lvgl_touch_indev = NULL;

    // Configuration structure for LVGL display port (binds LCD hardware to LVGL)
    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = lcd_io_handle,          // LCD panel IO handle (pre-initialized I2C/SPI IO)
        .panel_handle = panel_handle,        // LCD panel hardware handle (pre-initialized panel)
        .control_handle = NULL,              // Optional control handle (not used here, set to NULL)
        .buffer_size = LCD_H_RES * LCD_V_RES,// Size of LVGL display buffer 
        .double_buffer = true,               // Enable double buffering (reduces screen flicker)
        .trans_size = 0,                     // Transient buffer size (0 = disable transient buffer)
        .hres = LCD_H_RES,                   // LCD horizontal resolution (pixels)
        .vres = LCD_V_RES,                   // LCD vertical resolution (pixels)
        .monochrome = false,                 // LCD type: false = color display, true = monochrome
        .rotation = {                        // Display rotation/mirror configuration
            .swap_xy = 0,                    // Do not swap X and Y axes (no 90/270° rotation)                         
            .mirror_x = 1,                   // Mirror display horizontally (flip left-right)
            .mirror_y = 0,                   // Do not mirror display vertically (no flip top-bottom)
        },
        .flags = {                           // Advanced display flags
            .buff_dma = 0,                   // Disable DMA for buffer access (0 = off)
            .buff_spiram = 1,                // Use external PSRAM for display buffer (1 = on)
            .sw_rotate = 1,                  // Enable software-based display rotation (1 = on)
            .full_refresh = 0,               // Disable full-screen refresh (0 = partial refresh, saves bandwidth)
            .direct_mode = 0,                // Disable direct display mode (0 = use LVGL buffer mode)
        },
    };

    // Register the configured display device to LVGL and get the display handle
    lvgl_disp = lvgl_port_add_disp(&display_cfg);
    if(lvgl_disp != NULL)  // Check if display device registration is successful
    {
        // Configuration structure for LVGL touch port (binds touch hardware to LVGL)
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,               // Bind touch input to the registered LVGL display
            .handle = touch_handle,          // Touch controller hardware handle (pre-initialized)
        };
        // Register the touch device to LVGL and get the input device handle
        lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
    }
    else
    {
        ESP_LOGI(TAG, "lv_port_init ERROR");  // Log error if display device registration fails
    }
}

/* ----------------- RAM recording buffer initialization ----------------- */

/**
 * @brief Allocate and initialize the in-memory recording buffer.
 *
 * @return ESP_OK on success, ESP_FAIL on allocation failure.
 */
static esp_err_t record_buffer_init(void)
{
    if (s_record_buffer) {
        return ESP_OK;
    }

    // AFE output: 16 kHz / 16-bit / mono
    s_record_capacity = (size_t)RECORD_SECONDS * RECORD_SAMPLE_RATE * sizeof(int16_t);

    // Prefer allocating in PSRAM
    s_record_buffer = heap_caps_malloc(s_record_capacity,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_record_buffer) {
        ESP_LOGW(TAG, "record_buffer_init: PSRAM malloc %d failed, try internal RAM",
                 (int)s_record_capacity);
        s_record_buffer = heap_caps_malloc(s_record_capacity, MALLOC_CAP_8BIT);
    }

    if (!s_record_buffer) {
        ESP_LOGE(TAG, "record_buffer_init: malloc %d bytes failed", (int)s_record_capacity);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "record_buffer_init: capacity=%d bytes (~%.2f s @%dHz/16bit mono)",
             (int)s_record_capacity,
             (float)s_record_capacity / (RECORD_SAMPLE_RATE * 2.0f),
             RECORD_SAMPLE_RATE);
    return ESP_OK;
}

/* ----------------- Record task: mic_read -> AFE feed ----------------- */

/**
 * @brief FreeRTOS task for continuous microphone data acquisition and AFE feeding
 *
 * Core workflow:
 * 1. Waits indefinitely for AUDIO_EVENT_RECORD to be set in the event group
 * 2. Acquires mutex to ensure exclusive access to recording/playback resources
 * 3. Reads audio frames from microphone via mic_read() in a loop
 * 4. Feeds valid audio data into AFE (Audio Front-End) pipeline
 * 5. Stops recording when AUDIO_EVENT_RECORD is cleared
 * 6. Calculates and logs total recording duration before releasing mutex
 *
 * @param pvParameters FreeRTOS task parameter (unused in this task)
 */
static void record_task(void *pvParameters)
{
    (void)pvParameters; // Unused FreeRTOS task parameter, suppress compiler warning

    EventBits_t audio_record_task_event; // Bit field to store event group status
    // Get AFE feed parameters: chunk size (per channel), channel count
    int feed_chunksize = afe_handle->get_feed_chunksize(afe_data); // AFE required frame chunk size per channel
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);     // Number of channels for AFE feeding
    // Calculate buffer size: chunksize * channels * 2 bytes (int16_t per sample)
    int feed_buff_size = feed_chunksize * feed_nch * sizeof(int16_t);
    // Allocate buffer for audio data (int16_t samples) to feed AFE
    int16_t *feed_buff = (int16_t *)malloc(feed_buff_size);

    // Log AFE feed configuration for debugging
    ESP_LOGI(TAG, "feed_chunksize = %d", feed_chunksize);
    ESP_LOGI(TAG, "feed_nch = %d", feed_nch);
    ESP_LOGI(TAG, "feed_buff_size = %d", feed_buff_size);

    // Infinite task loop (FreeRTOS task never exits)
    while (1) {
        // Wait indefinitely for AUDIO_EVENT_RECORD to be set (block until event)
        audio_record_task_event = xEventGroupWaitBits(audio_event,
                                                      AUDIO_EVENT_RECORD,
                                                      pdFALSE,          // Don't clear the event bit on exit
                                                      pdFALSE,          // Wait for only AUDIO_EVENT_RECORD (not all bits)
                                                      portMAX_DELAY);   // No timeout - wait forever

        // Check if RECORD event is actually set (defensive check)
        if ((audio_record_task_event & AUDIO_EVENT_RECORD) == AUDIO_EVENT_RECORD) {

            // Acquire mutex to prevent race with playback task (block until mutex available)
            if (xSemaphoreTake(play_record_mutex, portMAX_DELAY) != pdTRUE) {
                ESP_LOGW(TAG, "Failed to take play_record_mutex, skip recording cycle");
                continue; // Skip this cycle if mutex acquisition fails
            }

            uint32_t total_audio_size = 0; // Total bytes of audio recorded in this session
            size_t bytes_read = 0;         // Bytes read from microphone in single mic_read() call

            ESP_LOGI(TAG, "Audio process task started (recording...)");

            // Inner loop: continuous read/feed until RECORD event is cleared
            while (1) {
                // Wait for RECORD/PLAY event (monitor for stop/playback trigger)
                audio_record_task_event = xEventGroupWaitBits(audio_event,
                                                              AUDIO_EVENT_PLAY | AUDIO_EVENT_RECORD,
                                                              pdFALSE,
                                                              pdFALSE,
                                                              portMAX_DELAY);
                
                // Exit recording loop if RECORD event is cleared (stop recording)
                if ((audio_record_task_event & AUDIO_EVENT_RECORD) != AUDIO_EVENT_RECORD) {
                    ESP_LOGI(TAG, "RECORD event cleared, stopping recording");
                    break;
                }

                // Read audio data from microphone into feed buffer
                if (mic_read(feed_buff, feed_buff_size, &bytes_read, portMAX_DELAY) == ESP_OK) {
                    // Only feed data if full buffer is read (validate data integrity)
                    if (bytes_read == (size_t)feed_buff_size) {
                        // Feed audio data to AFE pipeline
                        int feed_data_num = afe_handle->feed(afe_data, feed_buff);
                        // Log warning if AFE feed returns invalid/negative value
                        if (feed_data_num <= 0) {
                            ESP_LOGW(TAG, "afe_handle->feed returned %d (invalid data count)", feed_data_num);
                        }
                    } else {
                        // Incomplete buffer read - abort recording (likely error)
                        ESP_LOGW(TAG, "I2S bytes_read(%d) < feed_bytes(%d) - incomplete frame",
                                 (int)bytes_read, feed_buff_size);
                        break;
                    }
                    // Accumulate total recorded bytes
                    total_audio_size += bytes_read;
                } else {
                    ESP_LOGE(TAG, "mic_read() failed - aborting recording");
                    break; // Exit loop if mic read fails
                }
            }

            // Calculate approximate recording duration (seconds)
            // Formula: total bytes / (sample rate * bytes per sample * channels)
            // - 2.0f = 2 bytes per sample (int16_t)
            float seconds = (float)total_audio_size / (RECORD_SAMPLE_RATE * 2.0f * RECORD_CHANNELS);
            ESP_LOGI(TAG, "Record completed successfully | Total bytes: %ld | Duration: %.2f s",
                     (long)total_audio_size, seconds);

            // Release mutex to unlock recording/playback resources
            xSemaphoreGive(play_record_mutex);
        }
    }
}

/* ----------------- Playback task: loop playback from RAM buffer ----------------- */
/**
 * @brief FreeRTOS task for audio playback from recorded RAM buffer
 *
 * Core workflow:
 * 1. Waits indefinitely for AUDIO_EVENT_PLAY to be set (triggered by MIC button release/etc.)
 * 2. Acquires mutex to ensure exclusive access to recording/playback resources
 * 3. Waits up to 2 seconds for s_record_ready (signal from store_task that buffer is filled)
 * 4. Validates recorded data (buffer existence, non-zero length) before playback
 * 5. Enables speaker and plays audio in chunks from RAM buffer (loop playback)
 * 6. Stops playback if AUDIO_EVENT_PLAY is cleared or error occurs
 * 7. Disables speaker and releases mutex after playback stops
 *
 * @param pvParameters FreeRTOS task parameter (unused in this task)
 */
static void play_task(void *pvParameters)
{
    (void)pvParameters; // Unused FreeRTOS task parameter, suppress compiler warning

    EventBits_t audio_record_task_event; // Bit field to store audio event group status

    // Infinite task loop (FreeRTOS task never exits)
    while (1) {
        // Wait indefinitely for AUDIO_EVENT_PLAY to be set (block until play trigger)
        audio_record_task_event = xEventGroupWaitBits(audio_event,
                                                      AUDIO_EVENT_PLAY,
                                                      pdFALSE,          // Don't clear event bit on exit
                                                      pdFALSE,          // Wait only for PLAY event (not all bits)
                                                      portMAX_DELAY);   // No timeout - wait forever

        // Check if PLAY event is actually set (defensive validation)
        if ((audio_record_task_event & AUDIO_EVENT_PLAY) == AUDIO_EVENT_PLAY) {

            // Acquire mutex to prevent race with recording task (block until available)
            if (xSemaphoreTake(play_record_mutex, portMAX_DELAY) != pdTRUE) {
                ESP_LOGW(TAG, "play_task: Failed to take play_record_mutex, skip playback cycle");
                continue; // Skip this playback cycle if mutex acquisition fails
            }

            // Wait up to 2 seconds for store_task to finish filling the recording buffer
            int wait_ms = 0; // Elapsed wait time (milliseconds)
            while (!s_record_ready && wait_ms < 2000) {
                // Check if PLAY event was cleared externally (stop playback request)
                EventBits_t bits = xEventGroupGetBits(audio_event);
                if ((bits & AUDIO_EVENT_PLAY) == 0) {
                    ESP_LOGI(TAG, "play_task: PLAY event cleared during buffer wait");
                    break; // Exit wait loop if PLAY is no longer set
                }
                // Wait 20ms before rechecking s_record_ready (non-blocking delay)
                vTaskDelay(pdMS_TO_TICKS(20));
                wait_ms += 20; // Increment elapsed wait time
            }

            // Validate recorded data before playback (defensive check)
            // Conditions: buffer ready flag, non-zero data length, valid buffer pointer
            if (!s_record_ready || s_record_bytes == 0 || !s_record_buffer) {
                ESP_LOGW(TAG, "play_task: No valid recorded data (ready=%d, bytes=%u, buffer=%p) - skip playback",
                         s_record_ready, (unsigned int)s_record_bytes, s_record_buffer);
                xEventGroupClearBits(audio_event, AUDIO_EVENT_PLAY); // Clear PLAY event to avoid re-trigger
                xSemaphoreGive(play_record_mutex);                   // Release mutex before continuing
                continue;
            }

            // Log playback start with buffer metadata
            ESP_LOGI(TAG, "play_task: Start loop playback from RAM | Total bytes: %u",
                     (unsigned int)s_record_bytes);

            // Enable speaker hardware (power/configuration)
            ctrl_speaker_on();
            size_t offset = 0; // Current playback offset in the recording buffer (bytes)

            // Inner loop: continuous playback until stop condition
            while (1) {
                // Get current state of audio event group
                EventBits_t bits = xEventGroupGetBits(audio_event);

                // Stop playback if PLAY event is cleared (external stop trigger)
                if ((bits & AUDIO_EVENT_PLAY) == 0) {
                    ESP_LOGI(TAG, "play_task: PLAY bit cleared - stopping playback loop");
                    break;
                }

                // Restart playback from buffer start when end is reached (loop playback)
                if (offset >= s_record_bytes) {
                    ESP_LOGI(TAG, "play_task: Playback completed one loop - restarting from buffer start");
                    offset = 0; // Reset playback offset to beginning
                    vTaskDelay(pdMS_TO_TICKS(50)); // Short delay before restart (prevent rapid looping)
                    continue;
                }

                // Calculate remaining bytes in buffer and chunk size for playback
                size_t remain = s_record_bytes - offset; // Remaining bytes to play
                // Use DMA buffer size as chunk (max) or remaining bytes (if smaller)
                size_t chunk = (remain > DMA_BUF_LEN * sizeof(int16_t))
                                   ? DMA_BUF_LEN * sizeof(int16_t)
                                   : remain;

                size_t bytes_written = 0; // Bytes actually written to speaker
                // Play audio chunk from RAM buffer (block until completion/timeout)
                esp_err_t ret = speaker_play(
                    (int16_t *)((uint8_t *)s_record_buffer + offset), // Playback buffer pointer (offset adjusted)
                    chunk,                                           // Chunk size to play (bytes)
                    &bytes_written,                                   // Output: bytes successfully played
                    portMAX_DELAY);                                   // No timeout - wait for playback

                // Handle playback errors
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "play_task: speaker_play failed | Error code: %d", ret);
                    break; // Exit playback loop on error
                }
                // Handle zero-byte playback (unexpected, abort loop)
                if (bytes_written == 0) {
                    ESP_LOGW(TAG, "play_task: speaker_play wrote 0 bytes - aborting playback");
                    break;
                }

                // Advance playback offset by bytes successfully written
                offset += bytes_written;
            }

            // Disable speaker hardware after playback stops (power saving)
            ctrl_speaker_off();
            // Release mutex to unlock recording/playback resources
            xSemaphoreGive(play_record_mutex);
        }
    }
}


/* ----------------- Store task: AFE fetch -> RAM buffer ----------------- */

/**
 * @brief FreeRTOS task for storing AFE-processed audio data to RAM buffer
 *
 * Core workflow:
 * 1. Waits indefinitely for AUDIO_EVENT_RECORD to start recording
 * 2. Initializes recording state (resets buffer counter/ready flag)
 * 3. Continuously fetches processed audio data from AFE (Audio Front-End)
 * 4. Copies valid AFE data to RAM buffer (respects buffer capacity limits)
 * 5. Monitors "idle count" (AFE no-data periods) to detect recording end
 * 6. Stops storage when:
 *    - AUDIO_EVENT_RECORD is cleared (recording stopped) AND
 *    - AFE produces no data for >500ms (idle_count > 5 @ 100ms/check)
 * 7. Sets s_record_ready flag if valid data was stored (for playback task)
 *
 * @param pvParameters FreeRTOS task parameter (unused in this task)
 */
static void store_task(void *pvParameters)
{
    (void)pvParameters; // Unused FreeRTOS task parameter, suppress compiler warning

    // Infinite task loop (FreeRTOS task never exits)
    while (1) {
        // Wait indefinitely for AUDIO_EVENT_RECORD to start recording workflow
        xEventGroupWaitBits(audio_event,
                            AUDIO_EVENT_RECORD,
                            pdFALSE,          // Don't clear event bit on exit
                            pdFALSE,          // Wait only for RECORD event (not all bits)
                            portMAX_DELAY);   // No timeout - wait forever

        // Reset recording buffer state before new capture (critical for reusability)
        s_record_bytes = 0;    // Reset total recorded bytes counter
        s_record_ready = false;// Clear "buffer ready" flag for playback task

        // Log start of AFE data capture with buffer capacity info
        ESP_LOGI(TAG, "store_task: Start capturing AFE output to RAM | Buffer capacity: %d bytes",
                 (int)s_record_capacity);

        int idle_count = 0; // Counter for consecutive AFE "no data" cycles (100ms/cycle)

        // Inner loop: capture AFE data until recording stops + AFE is idle
        while (1) {
            // Fetch processed audio data from AFE (max 100ms wait per call)
            // pdMS_TO_TICKS(100): Convert 100ms to FreeRTOS system ticks
            afe_fetch_result_t *res = afe_handle->fetch_with_delay(afe_data, pdMS_TO_TICKS(100));

            // Handle invalid/missing AFE data (increment idle counter)
            if (!res || res->ret_value != ESP_OK || !res->data || res->data_size <= 0) {
                idle_count++; // No valid data - increment idle cycle counter
            } else {
                idle_count = 0; // Reset idle counter (valid data received)

                // Calculate bytes to copy (respect buffer capacity - prevent overflow)
                size_t copy_bytes = res->data_size;
                if (copy_bytes > (s_record_capacity - s_record_bytes)) {
                    copy_bytes = s_record_capacity - s_record_bytes; // Limit to remaining buffer space
                }

                // Copy valid AFE data to RAM buffer (if buffer exists and bytes to copy >0)
                if (copy_bytes > 0 && s_record_buffer) {
                    memcpy((uint8_t *)s_record_buffer + s_record_bytes, // Destination: buffer + current offset
                           res->data,                                   // Source: AFE processed data
                           copy_bytes);                                 // Number of bytes to copy
                    s_record_bytes += copy_bytes;                       // Update total recorded bytes
                }

                // Log warning if buffer is full (extra data will be dropped)
                if (s_record_bytes >= s_record_capacity) {
                    ESP_LOGW(TAG, "store_task: Recording buffer full (%d bytes) - dropping extra AFE data",
                             (int)s_record_capacity);
                }

                // Uncomment to inspect AFE ring buffer usage (debug purpose)
                // ESP_LOGI(TAG, "AFE ring buffer free percentage: %.3f", res->ringbuff_free_pct);
            }

            // Get current state of audio event group (check if recording stopped)
            EventBits_t bits = xEventGroupGetBits(audio_event);

            // Stop capture if:
            // 1. RECORD event is cleared (external stop trigger) AND
            // 2. AFE has no data for >500ms (idle_count > 5 @ 100ms per check)
            if ((bits & AUDIO_EVENT_RECORD) == 0 && idle_count > 5) {
                ESP_LOGI(TAG, "store_task: Recording stopped + AFE idle (idle_count=%d) - finish capture", idle_count);
                break; // Exit inner loop (end of recording)
            }
        }

        // Set "buffer ready" flag only if valid data was stored (non-zero bytes)
        s_record_ready = (s_record_bytes > 0);
        // Calculate approximate recording duration (seconds)
        // Formula: total bytes / (sample rate * 2 bytes per sample (int16_t))
        float seconds_out = (float)s_record_bytes / (RECORD_SAMPLE_RATE * 2.0f);
        // Log capture completion with total bytes and duration
        ESP_LOGI(TAG, "store_task: Capture finished | Total bytes: %u | Duration: %.2f s",
                 (unsigned int)s_record_bytes, seconds_out);
    }
}

/* ----------------- Button event callbacks ----------------- */

/**
 * @brief MIC button long press: start recording.
 */
static void btn_pressing_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "btn_pressing_cb");

    lv_obj_t *btn = lv_event_get_target(e);
    EventBits_t r_event = xEventGroupWaitBits(audio_event,
                                              AUDIO_EVENT_RECORD,
                                              pdFALSE,
                                              pdTRUE,
                                              0);

    ESP_LOGI(TAG, "r_event = 0x%lx", (unsigned long)r_event);

    if ((r_event & AUDIO_EVENT_RECORD) != AUDIO_EVENT_RECORD) {
        ESP_LOGI(TAG, "start recording");
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xE74C3C),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        xEventGroupSetBits(audio_event, AUDIO_EVENT_RECORD);
        xEventGroupClearBits(audio_event, AUDIO_EVENT_PLAY);
        ctrl_speaker_off();

        // Reset buffer state for a new recording
        s_record_ready = false;
        s_record_bytes = 0;
    }
}

/**
 * @brief MIC button release: stop recording and start loop playback.
 */
static void btn_released_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "btn_released_cb");

    lv_obj_t *btn = lv_event_get_target(e);
    EventBits_t bits = xEventGroupGetBits(audio_event);

    if (bits & AUDIO_EVENT_RECORD) {
        // Stop recording
        xEventGroupClearBits(audio_event, AUDIO_EVENT_RECORD);

        // Change button color to indicate recording has stopped
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2ECC71),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        // Trigger playback; play_task will loop until STOP clears PLAY
        xEventGroupSetBits(audio_event, AUDIO_EVENT_PLAY);
    }
}

/**
 * @brief STOP button: stop both recording and playback.
 */
static void stop_btn_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "stop_btn_event_cb");

    // Clear RECORD and PLAY flags
    EventBits_t r_event = xEventGroupWaitBits(audio_event,
                                              AUDIO_EVENT_RECORD | AUDIO_EVENT_PLAY,
                                              pdTRUE,
                                              pdFALSE,
                                              0);

    if ((r_event & (AUDIO_EVENT_RECORD | AUDIO_EVENT_PLAY)) != 0) {
        ctrl_speaker_off();
        if (bnt_mic) {
            lv_obj_set_style_bg_color(bnt_mic, lv_color_hex(0x2195F6),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

/* ----------------- UI initialization & task creation ----------------- */

/**
 * @brief Initialize the microphone test UI and create audio tasks.
 */
void microphone_ui_init(lv_obj_t *parent)
{
    if (audio_event == NULL) {
        audio_event = xEventGroupCreate();
        if (audio_event == NULL) {
            return;
        }
    }

    if (play_record_mutex == NULL) {
        play_record_mutex = xSemaphoreCreateMutex();
        if (play_record_mutex == NULL) {
            return;
        }
    }

    // Initialize microphone and speaker hardware
    init_pdm_microphone();
    speaker_Init();

    if (lvgl_port_lock(0)) {
        // Title label
        tite_label = lv_label_create(parent);
        lv_label_set_recolor(tite_label, true);
        lv_label_set_text(tite_label, "#808080 Microphone Test#");
        lv_obj_align(tite_label, LV_ALIGN_CENTER, 0, -50);

        // MIC button
        bnt_mic = lv_btn_create(parent);
        lv_obj_set_size(bnt_mic, 70, 40);
        lv_obj_align(bnt_mic, LV_ALIGN_CENTER, -60, 50);
        lv_obj_set_style_bg_color(bnt_mic, lv_color_hex(0x2195F6),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *bnt_mic_label = lv_label_create(bnt_mic);
        lv_obj_set_style_text_font(bnt_mic_label, &ui_mic_font, LV_PART_MAIN);
        lv_label_set_text(bnt_mic_label, MY_MIC_SYMBOL);
        lv_obj_center(bnt_mic_label);

        lv_obj_add_event_cb(bnt_mic, btn_pressing_cb, LV_EVENT_LONG_PRESSED, NULL);
        lv_obj_add_event_cb(bnt_mic, btn_released_cb, LV_EVENT_RELEASED, NULL);

        // STOP button
        bnt_stop = lv_btn_create(parent);
        lv_obj_set_size(bnt_stop, 70, 40);
        lv_obj_align(bnt_stop, LV_ALIGN_CENTER, 60, 50);

        lv_obj_t *bnt_stop_label = lv_label_create(bnt_stop);
        lv_label_set_text(bnt_stop_label, "Stop");
        lv_obj_center(bnt_stop_label);

        lv_obj_add_event_cb(bnt_stop, stop_btn_event_cb, LV_EVENT_CLICKED, NULL);

        lvgl_port_unlock();
    }

    // Initialize recording buffer
    if (record_buffer_init() != ESP_OK) {
        ESP_LOGE(TAG, "microphone_ui_init: record_buffer_init failed");
        if (tite_label) {
            lv_label_set_text(tite_label, "#ff0000 No RAM for record buffer#");
        }
        return;
    }
    // Create tasks
    xTaskCreate(play_task,               "play_task",   10 * 1024, NULL, 9, NULL);
    xTaskCreatePinnedToCore(record_task, "record_task", 15 * 1024, NULL, 9, NULL, 1);
    xTaskCreatePinnedToCore(store_task,  "store_task",   8 * 1024, NULL, 8, NULL, 0);
}

/* ----------------- app_main ----------------- */

/**
 * @brief Main application entry point.
 *
 * Initializes NVS, board peripherals, LVGL, AFE, and microphone UI.
 */
void app_main(void)
{
    // Initialize NVS (Non-Volatile Storage) - required for ESP32 system settings
    esp_err_t ret = nvs_flash_init();
    // Recover from common NVS errors:
    // - ESP_ERR_NVS_NO_FREE_PAGES: No free pages in NVS partition
    // - ESP_ERR_NVS_NEW_VERSION_FOUND: NVS partition version mismatch
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init failed (error: %d) - erase NVS flash and retry", ret);
        ESP_ERROR_CHECK(nvs_flash_erase()); // Erase NVS partition to resolve errors
        ret = nvs_flash_init();             // Re-initialize NVS after erase
    }
    ESP_ERROR_CHECK(ret); // Abort if NVS init still fails (critical system dependency)

    // Initialize board-level I2C bus (for peripheral communication: LCD/touch/PCF8574)
    board_iic_init();
    // Initialize PCF8574 I/O expander (extends GPIO for LCD/touch control)
    ESP_ERROR_CHECK(exio_pcf8574_init());
    // Initialize LCD display (core display hardware setup)
    ESP_ERROR_CHECK(Lcd_Init());

    // Initialize LCD brightness control module
    lcd_brightness_init();
    // Set LCD maximum brightness (100% - adjust per UI requirements)
    lcd_brightness_set(100);

    // Initialize CST826 touch screen controller (LVGL touch input backend)
    lcd_cst826_touch_Init();
    // Initialize LVGL port (bind LVGL to hardware: display/touch/clock)
    lv_port_init();

    // ------------------------------ AFE Initialization ------------------------------
    // AFE (Audio Front-End): Core audio processing pipeline for speech enhancement
    // Initialize speech recognition model list (load models from "model" partition)
    srmodel_list_t *models = esp_srmodel_init("model");
    if (models != NULL) {
        // Create AFE configuration with:
        // - "MM": Module manager identifier
        // - models: Preloaded speech recognition model list
        // - AFE_TYPE_SR: AFE type = Speech Recognition
        // - AFE_MODE_HIGH_PERF: High performance mode (prioritize audio quality over CPU)
        afe_config_t *afe_config = afe_config_init("MM", models,
                                                AFE_TYPE_SR,
                                                AFE_MODE_HIGH_PERF);

        // Configure AFE task priority and core affinity (FreeRTOS)
        afe_config->afe_perferred_priority = 5;  // Set AFE task priority (5 = moderate/high)
        afe_config->afe_perferred_core = 0;      // Run AFE task on core 0 (core 1 for app logic)
        // Allocate AFE memory from PSRAM (prioritize PSRAM to save internal RAM)
        afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

        // Filter model list to get Noise Suppression (NS) model name (prefix: ESP_NSNET_PREFIX)
        char *ns_model_name  = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
        // Filter model list to get Voice Activity Detection (VAD) model name (prefix: ESP_VADN_PREFIX)
        char *vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, NULL);

        // Configure Noise Suppression (NS) module
        if (ns_model_name != NULL) {
            afe_config->ns_init = true;               // Enable NS module
            afe_config->ns_model_name = ns_model_name;// Assign NS model
            afe_config->afe_ns_mode = AFE_NS_MODE_NET;// Use neural network-based NS (better performance)
            ESP_LOGW(TAG, "Noise Suppression (NS) model loaded - enabling NS");
        } else {
            afe_config->ns_init = false; // Disable NS if model not found
            ESP_LOGW(TAG, "NS model not found - disabling Noise Suppression");
        }

        // Configure Voice Activity Detection (VAD) module
        if (vad_model_name != NULL) {
            afe_config->vad_model_name = vad_model_name; // Assign VAD model (enable by default)
            ESP_LOGI(TAG, "VAD model loaded: %s", vad_model_name);
        } else {
            ESP_LOGW(TAG, "VAD model not found - VAD may not function");
        }

        // Configure AFE module enable/disable (tailor to audio use case)
        afe_config->wakenet_init = false;   // Disable WakeNet (no wake word detection needed)
        afe_config->aec_init      = false;  // Disable AEC (Acoustic Echo Cancellation - no speaker echo)
        afe_config->vad_init      = true;   // Enable VAD (filter silence for cleaner audio output)

        afe_config->agc_init      = true;   // Enable AGC (Automatic Gain Control - normalize volume)
        afe_config->se_init       = true;   // Enable SE (Speech Enhancement - improve speech clarity)

        // Tune AFE linear gain (reduce to 4.0f to avoid amplifying background noise)
        // Higher gain = louder audio, but risk of noise boosting; lower = cleaner but quieter
        afe_config->afe_linear_gain = 4.0f;

        // Print full AFE configuration (debug: verify all settings)
        afe_config_print(afe_config);
        // Create AFE handle from configuration (core AFE instance)
        afe_handle = esp_afe_handle_from_config(afe_config);
        // Create AFE data context (runtime state for AFE processing)
        afe_data = afe_handle->create_from_config(afe_config);
        // Free AFE config memory (no longer needed after handle creation)
        afe_config_free(afe_config);
    } else {
        ESP_LOGE(TAG, "Failed to initialize speech recognition model list - AFE disabled");
    }

    // Initialize microphone control UI (LVGL-based UI elements on main screen)
    microphone_ui_init(lv_scr_act());
}
