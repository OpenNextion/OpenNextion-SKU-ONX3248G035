#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_common.h"
#include "driver/ledc.h"
#include "esp_err.h"



#include "esp_lcd_touch_cst826.h"
#include "esp_lcd_st7796u.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "exio_pcf8574.h"

#include "wifi_manager.h"

#include "gui.h"

#define NEXTION_IIC_SCL_PIN  7
#define NEXTION_IIC_SDA_PIN  8

#define LCD_H_RES    (320)    
#define LCD_V_RES    (480) 

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


const char *TAG = "WIFI_TEST";


/**
 * @brief Static handle for I2C master bus 0
 * 
 * This handle manages the I2C master bus 0 (I2C_NUM_0) on ESP32, which is used for 
 * communication with I2C peripherals (e.g., PCF8574 expander, CST826 touch controller). 
 * It is initialized in board-level I2C initialization and reused by other I2C devices.
 */
static i2c_master_bus_handle_t     esp32_i2c_bus_num0_handle = NULL;

/**
 * @brief Static handle for the LCD touch controller
 * 
 * This handle represents the touch controller (e.g., CST826) instance, used to 
 * interact with the touch panel (e.g., read touch coordinates, check press status). 
 * It is initialized in touch controller initialization and bound to LVGL for input handling.
 */
static esp_lcd_touch_handle_t      touch_handle = NULL;

/**
 * @brief Static handle for the LCD panel hardware
 * 
 * This handle manages the LCD panel (e.g., ST7796U) hardware operations, including 
 * panel initialization, reset, display on/off, color inversion, and low-level drawing. 
 * It is created when initializing the SPI LCD and used by LVGL for screen rendering.
 */
static esp_lcd_panel_handle_t      panel_handle = NULL;

/**
 * @brief Static handle for LCD panel IO interface
 * 
 * This handle controls the low-level IO communication (e.g., SPI) between ESP32 and the LCD panel. 
 * It handles data/command transmission over the SPI bus, including timing and signal control 
 * (e.g., DC, CS pins). Required for panel initialization and rendering operations.
 */
static esp_lcd_panel_io_handle_t   lcd_io_handle = NULL;   




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
        .clk_source = I2C_CLK_SRC_DEFAULT,        // Use default I2C clock source (typically APB clock)
        .i2c_port = I2C_NUM_0,                    // I2C port number (0 in this case)
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
        .pclk_hz             = 80 * 1000 * 1000,    /* SPI clock frequency (15 MHz) */
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

    // LVGL display device handle (stores the registered display in LVGL)
    static lv_display_t *lvgl_disp = NULL;
    // LVGL touch input device handle (stores the registered touch in LVGL)
    static lv_indev_t *lvgl_touch_indev = NULL;

    // Configuration structure for LVGL display port (binds LCD hardware to LVGL)
    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = lcd_io_handle,          // LCD panel IO handle (pre-initialized I2C/SPI IO)
        .panel_handle = panel_handle,        // LCD panel hardware handle (pre-initialized panel)
        .control_handle = NULL,              // Optional control handle (not used here, set to NULL)
        .buffer_size = LCD_H_RES * LCD_V_RES / 4,  // Size of LVGL display buffer (calculated as: resolution / 4 bytes)
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







void app_main(void)
{
    esp_err_t ret = nvs_flash_init();  // Initialize Non-Volatile Storage (NVS) for system/user data
    // Handle NVS initialization errors: erase NVS if no free pages or version mismatch
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());  // Erase NVS to resolve initialization issues
        ret = nvs_flash_init();              // Re-initialize NVS after erasure
    }
    ESP_ERROR_CHECK(ret);               // Abort if NVS initialization fails

    board_iic_init();                  // Initialize board-level I2C master bus (I2C_NUM_0)
    ESP_ERROR_CHECK(exio_pcf8574_init());  // Initialize PCF8574 I2C expander (for peripheral control pins)
    ESP_ERROR_CHECK(Lcd_Init());     // Initialize ST7796U LCD panel via SPI interface

    lcd_brightness_init();                    // Initialize LCD brightness 
    lcd_brightness_set(100);                  // Set LCD brightness to maximum brightness (100%)
    
    lcd_cst826_touch_Init();                   // Initialize CST826 touch controller via I2C
    lv_port_init();                    // Initialize LVGL port (bind LCD/touch to LVGL framework)
    
    wifi_manager_init();    // Initialize WiFi manager module (STA mode, scan/connect logic, event handling)
    gui_init(); // Initialize LVGL-based GUI (WiFi management UI, event callbacks, UI components)
}