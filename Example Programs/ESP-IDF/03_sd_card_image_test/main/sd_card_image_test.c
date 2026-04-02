#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <sys/unistd.h>
#include <sys/stat.h>

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"


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

#include "lv_fs_if.h"

#include "esp_lv_decoder.h"
#define NEXTION_IIC_SCL_PIN  7
#define NEXTION_IIC_SDA_PIN  8

#define LCD_H_RES    (320)    
#define LCD_V_RES    (480) 

#define MAX_FILENAME_LENGTH    256
#define MAX_FILES              100

#define LCD_HOST               SPI2_HOST

#define EXAMPLE_PIN_SD_CMD     GPIO_NUM_10
#define EXAMPLE_PIN_SD_D0      GPIO_NUM_9
#define EXAMPLE_PIN_SD_CLK     GPIO_NUM_11

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


const char *TAG = "SD_CARD_IMAGE_TEST";



#define SD_MOUNT_POINT          "/sdcard"
#define LVGL_IMAGES_DIR         "S:/images"          /* LVGL drive letter path */

/* ---------------- LVGL v8/v9 compatibility helpers ---------------- */
/* ======================= LVGL image API wrappers (LVGL 8.x) ======================= */
#define UI_IMAGE_CREATE(parent)          lv_img_create(parent)
#define UI_IMAGE_SET_SRC(obj, src)       lv_img_set_src(obj, src)
#define UI_IMAGE_SET_SCALE(obj, s)       lv_img_set_zoom(obj, s)   /* 256 = 1.0x */
#define UI_IMAGE_SET_PIVOT(obj, x, y)    lv_img_set_pivot(obj, x, y)
#define UI_IMG_GET_INFO(src, header)     lv_img_decoder_get_info(src, header)
typedef lv_img_header_t                 ui_image_header_t;
static inline void ui_get_disp_res(uint32_t *w, uint32_t *h)
{
    lv_disp_t *d = lv_disp_get_default();
    *w = lv_disp_get_hor_res(d);
    *h = lv_disp_get_ver_res(d);
}
/* ================================================================================ */

#define UI_SCALE_1X  (256U)

static lv_obj_t *img_cont = NULL;
static int s_img_index = 0;
static bool s_cover_mode = false; /* false=FIT, true=COVER */

static esp_lv_decoder_handle_t s_decoder = NULL;
static lv_obj_t *img = NULL;
static lv_obj_t *label = NULL;
static char image_array[MAX_FILES][MAX_FILENAME_LENGTH];
static int find_image_count = 0;



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


static  sdmmc_card_t *card = NULL;

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





static esp_err_t esp_sdcard_port_init(void)
{
    esp_err_t ret;

    exio_pcf8574_pin_write(EXIO_PCF8574_PIN_SDCS, EXIO_PCF8574_LEVEL_H);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 32 * 1024};
    const char mount_point[] = "/sdcard";
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    ESP_LOGI(TAG, "Using SDMMC peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    slot_config.width = 1;

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
    slot_config.clk = EXAMPLE_PIN_SD_CLK;
    slot_config.cmd = EXAMPLE_PIN_SD_CMD;
    slot_config.d0 = EXAMPLE_PIN_SD_D0;

    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        return ret;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    return ESP_OK;
}





/**
 * @brief Check if a file is an image file based on its extension
 * 
 * This function determines whether a given file is an image by examining its filename extension.
 * It checks for common image file extensions (case-sensitive comparison).
 * 
 * @param filename The full or relative path/name of the file to check
 * @return true if the file has a recognized image extension, false otherwise
 */

static bool ends_with_icase(const char *s, const char *suffix)
{
    if(!s || !suffix) return false;
    size_t ls = strlen(s);
    size_t le = strlen(suffix);
    if(ls < le) return false;

    const char *p = s + (ls - le);
    for(size_t i = 0; i < le; i++) {
        char a = p[i];
        char b = suffix[i];
        a = (char)tolower((unsigned char)a);
        b = (char)tolower((unsigned char)b);
        if(a != b) return false;
    }
    return true;
}


/* ---------------- JPEG compatibility helper ----------------
 * Many embedded JPEG decoders (including TJpgDec) only support Baseline JPEG.
 * Progressive JPEG (SOF2) is a common reason why a ".jpeg" file cannot be decoded.
 *
 * This probe reads JPEG markers from the file via LVGL FS to detect baseline/progressive.
 * It does NOT fully validate the file, only tries to detect SOF0 (baseline) vs SOF2 (progressive).
 */
typedef enum {
    JPEG_PROBE_UNKNOWN = 0,
    JPEG_PROBE_BASELINE,
    JPEG_PROBE_PROGRESSIVE,
    JPEG_PROBE_NOT_JPEG,
    JPEG_PROBE_IO_ERROR
} jpeg_probe_t;

static jpeg_probe_t jpeg_probe_baseline_or_progressive(const char *lvgl_path)
{
    if(!lvgl_path) return JPEG_PROBE_UNKNOWN;

    lv_fs_file_t f;
    lv_fs_res_t r = lv_fs_open(&f, lvgl_path, LV_FS_MODE_RD);
    if(r != LV_FS_RES_OK) {
        ESP_LOGW(TAG, "jpeg_probe: open failed: %s (res=%d)", lvgl_path, (int)r);
        return JPEG_PROBE_IO_ERROR;
    }

    /* Read SOI */
    uint8_t soi[2] = {0};
    uint32_t br = 0;
    r = lv_fs_read(&f, soi, 2, &br);
    if(r != LV_FS_RES_OK || br != 2) {
        lv_fs_close(&f);
        return JPEG_PROBE_IO_ERROR;
    }
    if(soi[0] != 0xFF || soi[1] != 0xD8) {
        lv_fs_close(&f);
        return JPEG_PROBE_NOT_JPEG;
    }

    while(1) {
        /* Find 0xFF marker prefix */
        uint8_t b = 0;
        do {
            br = 0;
            r = lv_fs_read(&f, &b, 1, &br);
            if(r != LV_FS_RES_OK || br != 1) {
                lv_fs_close(&f);
                return JPEG_PROBE_UNKNOWN;
            }
        } while(b != 0xFF);

        /* Skip fill bytes 0xFF */
        uint8_t marker = 0;
        do {
            br = 0;
            r = lv_fs_read(&f, &marker, 1, &br);
            if(r != LV_FS_RES_OK || br != 1) {
                lv_fs_close(&f);
                return JPEG_PROBE_UNKNOWN;
            }
        } while(marker == 0xFF);

        /* Start of Scan / End of Image => stop scanning */
        if(marker == 0xDA || marker == 0xD9) {
            lv_fs_close(&f);
            return JPEG_PROBE_UNKNOWN;
        }

        /* Restart markers, TEM, SOI have no length */
        if((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01 || marker == 0xD8) {
            continue;
        }

        /* Read segment length */
        uint8_t lenbuf[2] = {0};
        br = 0;
        r = lv_fs_read(&f, lenbuf, 2, &br);
        if(r != LV_FS_RES_OK || br != 2) {
            lv_fs_close(&f);
            return JPEG_PROBE_UNKNOWN;
        }
        uint16_t seglen = (uint16_t)((lenbuf[0] << 8) | lenbuf[1]);
        if(seglen < 2) {
            lv_fs_close(&f);
            return JPEG_PROBE_UNKNOWN;
        }

        /* SOF markers tell us baseline vs progressive */
        if(marker == 0xC0) { /* SOF0 */
            lv_fs_close(&f);
            return JPEG_PROBE_BASELINE;
        }
        if(marker == 0xC2) { /* SOF2 */
            lv_fs_close(&f);
            return JPEG_PROBE_PROGRESSIVE;
        }

        /* Skip rest of this segment */
        lv_fs_seek(&f, (uint32_t)(seglen - 2), LV_FS_SEEK_CUR);
    }
}


/**
 * @brief Determine whether a filename looks like a supported image file.
 *
 * Note:
 * - This is only for *your directory scanning filter*.
 * - Actual decode support depends on registered LVGL decoders / esp_lv_decoder.
 */
static bool is_image_file(const char *filename)
{
    return ends_with_icase(filename, ".bin")  ||
           ends_with_icase(filename, ".jpg")  ||
           ends_with_icase(filename, ".jpeg") ||
           ends_with_icase(filename, ".png")  ||
           ends_with_icase(filename, ".bmp")  ||
           ends_with_icase(filename, ".qoi")  ||
           ends_with_icase(filename, ".sjpg") ||
           ends_with_icase(filename, ".spng") ||
           ends_with_icase(filename, ".sqoi");
}



/**
 * @brief Scan a specified directory and collect names of image files
 * 
 * This function opens the target directory, reads all entries (files), filters out image files
 * using is_image_file(), and stores their names into the global image_array. It stops collecting
 * when the number of found images reaches MAX_FILES.
 * 
 * @param dir_path Path of the directory to scan (e.g., "/sdcard/images")
 * @note Depends on global variables: 
 *       - image_array: Array to store collected image filenames
 *       - find_image_count: Counter for the number of collected image files
 *       - MAX_FILES: Maximum number of image files that can be stored
 *       - MAX_FILENAME_LENGTH: Maximum length of each filename in image_array
 */

static int cmp_filename_icase(const void *a, const void *b)
{
    const char *sa = (const char *)a;
    const char *sb = (const char *)b;
    /* a/b are pointers to char[MAX_FILENAME_LENGTH] blocks */
    const char *pa = sa;
    const char *pb = sb;
    while(*pa && *pb) {
        char ca = (char)tolower((unsigned char)*pa++);
        char cb = (char)tolower((unsigned char)*pb++);
        if(ca != cb) return (int)ca - (int)cb;
    }
    return (int)tolower((unsigned char)*pa) - (int)tolower((unsigned char)*pb);
}

/**
 * @brief Scan SD image directory using LVGL FS and collect filenames into image_array[].
 *
 * Why LVGL FS?
 * - Your project already uses LVGL's file system layer (lv_fs_*) to browse the SD drive.
 *
 *   to workaround decoders which only match ".jpg" extension.
 */
void scan_images_directory(const char *dir_path)
{
    find_image_count = 0;
    memset(image_array, 0, sizeof(image_array));

    lv_fs_dir_t dir;
    lv_fs_res_t res = lv_fs_dir_open(&dir, dir_path);
    if(res != LV_FS_RES_OK) {
        ESP_LOGE(TAG, "scan_images_directory: open '%s' failed, res=%d", dir_path, (int)res);
        return;
    }

    char filename[MAX_FILENAME_LENGTH];

    while(find_image_count < MAX_FILES &&
          (lv_fs_dir_read(&dir, filename) == LV_FS_RES_OK) &&
          filename[0] != '\0')
    {
        /* Skip current/parent directory entries if any */
        if(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
            continue;
        }

        if(!is_image_file(filename)) {
            continue;
        }

        strncpy(image_array[find_image_count], filename, MAX_FILENAME_LENGTH - 1);
        image_array[find_image_count][MAX_FILENAME_LENGTH - 1] = '\0';
        find_image_count++;
    }

    lv_fs_dir_close(&dir);

    /* Make browsing stable: sort filenames */
    if(find_image_count > 1) {
        qsort(image_array, find_image_count, MAX_FILENAME_LENGTH, cmp_filename_icase);
    }

    ESP_LOGI(TAG, "scan_images_directory: found %d image(s) in %s", find_image_count, dir_path);
}





/**
 * @brief Callback function to handle gesture events on an image object
 * 
 * This function responds to gesture events (specifically left/right swipes) on an image object.
 * It switches the displayed image by updating the image index cyclically (wrapping around when reaching bounds)
 * and refreshes the image source accordingly.
 * 
 * @param e Pointer to the LVGL event structure, containing event details (e.g., event code, target object)
 * @note Uses static variable `img_index` to track the currently displayed image index
 * @note Depends on global variables:
 *       - find_image_count: Total number of available image files (from scanned results)
 *       - image_array: Array storing filenames of scanned image files
 *       - img: The LVGL image object whose source will be updated
 */

static void build_lvgl_image_path(int idx, char *out, size_t out_len)
{
    if(!out || out_len == 0) return;
    if(idx < 0 || idx >= find_image_count) {
        snprintf(out, out_len, "%s", "");
        return;
    }
    snprintf(out, out_len, LVGL_IMAGES_DIR "/%s", image_array[idx]);
}

static uint32_t ui_calc_scale(uint32_t img_w, uint32_t img_h, uint32_t box_w, uint32_t box_h, bool cover)
{
    if(img_w == 0 || img_h == 0) return UI_SCALE_1X;

    float rw = (float)box_w / (float)img_w;
    float rh = (float)box_h / (float)img_h;
    float r  = cover ? (rw > rh ? rw : rh) : (rw < rh ? rw : rh);

    /* Don't upscale by default */
    if(r > 1.0f) r = 1.0f;
    uint32_t s = (uint32_t)(r * (float)UI_SCALE_1X + 0.5f);
    if(s < 1) s = 1;
    return s;
}

static bool ui_get_image_info(const char *src, ui_image_header_t *header)
{
    lv_res_t r = UI_IMG_GET_INFO(src, header);
    if(r != LV_RES_OK) {
        ESP_LOGW(TAG, "image get_info failed: %s (res=%d)", src, (int)r);
        return false;
    }
    return true;
}

static void ui_apply_scale(lv_obj_t *img_obj, const ui_image_header_t *header)
{
    uint32_t sw, sh;
    ui_get_disp_res(&sw, &sh);

    uint32_t scale = ui_calc_scale(header->w, header->h, sw, sh, s_cover_mode);

    UI_IMAGE_SET_SCALE(img_obj, scale);
    UI_IMAGE_SET_PIVOT(img_obj, (int32_t)(header->w / 2), (int32_t)(header->h / 2));
    lv_obj_center(img_obj);

    ESP_LOGI(TAG, "scale_mode=%s, img=%ux%u, disp=%ux%u, scale=%u/256",
             s_cover_mode ? "COVER" : "FIT",
             (unsigned)header->w, (unsigned)header->h,
             (unsigned)sw, (unsigned)sh,
             (unsigned)scale);
}

static void ui_show_image(int idx)
{
    if(!img) return;
    if(find_image_count <= 0) return;

    if(idx < 0) idx = 0;
    if(idx >= find_image_count) idx = find_image_count - 1;
    s_img_index = idx;

    char path[MAX_FILENAME_LENGTH + 32];
    build_lvgl_image_path(s_img_index, path, sizeof(path));

    ui_image_header_t header;
    if(ui_get_image_info(path, &header)) {
        UI_IMAGE_SET_SRC(img, path);
        ui_apply_scale(img, &header);
    } else {
        /* Try still set src, so you can see any LVGL error log on console */
        UI_IMAGE_SET_SRC(img, path);
    }
}

static void img_click_event_cb(lv_event_t *e)
{
    (void)e;
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_cover_mode = !s_cover_mode;
    ui_show_image(s_img_index);
}

static void img_gesture_event_cb(lv_event_t *e)
{
    
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    if(dir == LV_DIR_LEFT) {
        s_img_index++;
        if(s_img_index >= find_image_count) s_img_index = 0;
        lv_indev_wait_release(lv_indev_get_act());
        ui_show_image(s_img_index);
    } else if(dir == LV_DIR_RIGHT) {
        s_img_index--;
        if(s_img_index < 0) s_img_index = find_image_count - 1;
        lv_indev_wait_release(lv_indev_get_act());
        ui_show_image(s_img_index);
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

    lcd_cst826_touch_Init();                   // Initialize CST826 touch controller via I2C
    lv_port_init();                    // Initialize LVGL port (bind LCD/touch to LVGL framework)

    /* Register Espressif LVGL image decoders (JPG/JPEG/PNG/QOI + split formats). */
    ESP_ERROR_CHECK(esp_lv_decoder_init(&s_decoder));
    lv_fs_if_init();
    lcd_brightness_init();                    // Initialize LCD brightness 
    lcd_brightness_set(100);                  // Set LCD brightness to maximum brightness (100%)
    

    if (esp_sdcard_port_init() != ESP_OK)
    {  
        if (lvgl_port_lock(0))
        {
            // No image files found: create a label to display an error message
            label = lv_label_create(lv_scr_act());
            // Align the label to the center of the screen
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_size(label, 250, 100);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_label_set_text(label, " No SD card detected. Please insert it and then power on again.");  
            lv_obj_set_style_text_color(label, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);   
        }

        lvgl_port_unlock();

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /**
     * Scan the "S:/images" directory to collect image filenames (populates image_array and find_image_count)
     * "S:" typically represents the SD card filesystem in LVGL port configurations
     */
    scan_images_directory(LVGL_IMAGES_DIR);

    /**
     * Acquire LVGL port lock to ensure thread-safe access to LVGL objects
     * 0 as parameter likely means no timeout (wait indefinitely until lock is acquired)
     */
    if (lvgl_port_lock(0))
    {
        // Check if any image files were found during the directory scan
        if(find_image_count > 0)
        {

            /* Container covers the screen; in COVER mode it will naturally crop overflow */
            img_cont = lv_obj_create(lv_scr_act());
            lv_obj_remove_style_all(img_cont);
            lv_obj_set_size(img_cont, lv_obj_get_width(lv_scr_act()), lv_obj_get_height(lv_scr_act()));
            lv_obj_center(img_cont);
            lv_obj_clear_flag(img_cont, LV_OBJ_FLAG_SCROLLABLE);
                   

            /* Create image inside container (use v8/v9 compatible macro) */
            img = UI_IMAGE_CREATE(img_cont);
            lv_obj_center(img);
            lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);

            /* Tap image to toggle FIT/COVER */
            lv_obj_add_event_cb(img, img_click_event_cb, LV_EVENT_CLICKED, NULL);

            /* Swipe left/right on screen to switch images */
            lv_obj_add_flag(img, LV_OBJ_FLAG_GESTURE_BUBBLE);
            lv_obj_add_event_cb(lv_scr_act(), img_gesture_event_cb, LV_EVENT_GESTURE, NULL);

            s_img_index = 0;
            ui_show_image(s_img_index);
        }
        else
        {
            // No image files found: create a label to display an error message
            label = lv_label_create(lv_scr_act());
            // Align the label to the center of the screen
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
            // Enable text recoloring (allows color codes in the text, e.g., #ff0000)
            lv_label_set_recolor(label, true);   
            // Set the label text to a red error message indicating no images found on SD card
            lv_label_set_text(label, "#ff0000  Image not found. #");  
        }

        // Release the LVGL port lock to allow other threads to access LVGL
        lvgl_port_unlock();
    }
}