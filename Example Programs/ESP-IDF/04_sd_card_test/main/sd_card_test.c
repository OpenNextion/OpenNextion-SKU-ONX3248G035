#include <stdio.h>
#include <string.h>

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


#define SD_MOUNT_POINT      "/sdcard"
#define RECORD_FILENAME     SD_MOUNT_POINT"/SD CARD TEST.txt"


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


const char *TAG = "SD_CARD_TEST";


static lv_obj_t *bnt = NULL;
static lv_obj_t *bnt_label = NULL;
static lv_obj_t *result_label = NULL;
static bool   s_sd_mounted = false;

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



/**
 * @brief  Initialize SD card port and mount FAT filesystem
 * @note   This function will check if SD card is already mounted first to avoid duplicate operation
 * @note   It configures SDMMC host/slot parameters and mounts the filesystem to specified mount point
 * @return esp_err_t - ESP_OK on success, corresponding error code on failure
 */
static esp_err_t esp_sdcard_port_init(void)
{
    esp_err_t ret;

    // Check if SD card is already mounted, return success directly if true
    if (s_sd_mounted) {
        return ESP_OK;
    }

    // Set SD card CS (Chip Select) pin to high level (deselect state)
    exio_pcf8574_pin_write(EXIO_PCF8574_PIN_SDCS, EXIO_PCF8574_LEVEL_H);
    // Delay 50ms to ensure stable pin state transition
    vTaskDelay(pdMS_TO_TICKS(50));

    // Configure FAT filesystem mount parameters
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,  // Do not format SD card automatically if mount fails
        .max_files = 5,                   // Maximum number of simultaneously open files
        .allocation_unit_size = 16 * 1024 // Allocation unit size (16KB) for FAT filesystem
    };

    // Log the start of SD card initialization process
    ESP_LOGI(TAG, "Initializing SD card");

    // Initialize SDMMC host with default configuration
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // Initialize SDMMC slot configuration with default settings
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Set SD card bus width to 1-bit (compatible with SPI mode or single-line SD mode)
    slot_config.width = 1;

    // Assign custom hardware pins for SD card communication (CLK/CMD/D0)
    slot_config.clk = EXAMPLE_PIN_SD_CLK;
    slot_config.cmd = EXAMPLE_PIN_SD_CMD;
    slot_config.d0 = EXAMPLE_PIN_SD_D0;
    // Enable internal pull-up resistors for SD card pins to ensure signal stability
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // Log the filesystem mount attempt with specified mount point path
    ESP_LOGI(TAG, "Mounting filesystem at %s", SD_MOUNT_POINT);
    // Mount FAT filesystem to the SD card and bind to the specified mount point
    ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    // Handle SD card mount failure
    if (ret != ESP_OK)
    {
        // Log warning message with detailed error description
        ESP_LOGW(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        card = NULL;                  // Clear SD card handle
        s_sd_mounted = false;         // Mark SD card as unmounted state
        // De-initialize SDMMC host to reset resources for subsequent mount retries
        sdmmc_host_deinit();
        return ret;                   // Return error code
    }

    s_sd_mounted = true;  // Mark SD card as successfully mounted
    ESP_LOGI(TAG, "Filesystem mounted");  // Log successful mount information
    sdmmc_card_print_info(stdout, card);  // Print detailed SD card hardware information to standard output
    return ESP_OK;        // Return success code
}

/**
 * @brief  De-initialize SD card port and unmount FAT filesystem
 * @note   This function will safely unmount the filesystem and release SDMMC host resources
 * @note   It returns early if SD card is not mounted initially
 * @return None
 */
static void esp_sdcard_port_deinit(void)
{
    // Return early if SD card is not in mounted state
    if (!s_sd_mounted) {
        return;
    }

    // Log the start of SD card unmount process
    ESP_LOGI(TAG, "Unmounting SD card");

    // Unmount FAT filesystem only if SD card handle is valid
    if (card != NULL) {
        // Attempt to unmount the filesystem from specified mount point
        esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
        // Log warning if unmount operation fails
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "esp_vfs_fat_sdcard_unmount failed: %s", esp_err_to_name(ret));
        }
    }

    card = NULL;                  // Clear SD card handle after unmount
    s_sd_mounted = false;         // Mark SD card as unmounted state
    // De-initialize SDMMC host to free up hardware resources (ensures reliable hot-plug re-mount)
    sdmmc_host_deinit();
}


static bool sdcard_card_ok_locked(void)
{
    if (!s_sd_mounted || card == NULL) {
        return false;
    }
    // CMD13 probe: if card is removed, this usually returns an error
    return (sdmmc_get_status(card) == ESP_OK);
}

static bool sdcard_ensure_ready_locked(void)
{
    // If mounted and still responding, keep it
    if (sdcard_card_ok_locked()) {
        return true;
    }

    // If it was mounted but card is gone, cleanup and re-mount
    if (s_sd_mounted) {
        esp_sdcard_port_deinit();
    }
    return (esp_sdcard_port_init() == ESP_OK);
}


/**
 * @brief LVGL button click callback function to test SD card read/write functionality
 * @param e LVGL event structure (contains event target and related data)
 * 
 * Core workflow (unchanged):
 * 1. Create a test file on SD card (overwrite if exists)
 * 2. Write a unique string with random number to the file
 * 3. Read the file back immediately
 * 4. Verify if read content matches written content
 * 5. Update button color and label to reflect test result (success/failure)
 * 
 * Optimizations:
 * - Fixed non-standard fwrite() parameter order (critical for correct byte writing)
 * - Replaced global buffer with local array (avoids thread safety issues)
 * - Added fsync() to ensure data is physically written to SD card (eliminates cache-related mismatches)
 * - Enhanced error checking for all I/O operations (fwrite, fflush, fsync, fread)
 * - Ensured thread-safe LVGL UI updates with lock/unlock
 * - Used snprintf() instead of sprintf() to prevent buffer overflow
 */
static void bnt_event_cb(lv_event_t *e)
{
    int rand_num = 0;                                  // Random number for test string uniqueness
    char save_str[64] = {0};                           // Local buffer for written content (avoids global dependency)
    char read_buf[64] = {0};                           // Buffer for storing read content from SD card
    size_t write_len = 0;                              // Length of written content
    size_t actual_written = 0;                         // Actual bytes written to file
    size_t actual_read = 0;                            // Actual bytes read from file

    bool is_there_sd = false;
    bool test_success = false;


    FILE *write_fp = NULL;                             // File pointer for writing
    FILE *read_fp = NULL;                              // File pointer for reading
    lv_obj_t *btn = lv_event_get_target(e);            // Get button object from event (avoids global btn dependency)
    lv_obj_t *btn_label = lv_obj_get_child(btn, 0);     // Get button's label (assumes label is first child)
    

    if(sdcard_ensure_ready_locked() == false)
    {
        is_there_sd = false;
        goto cleanup;
    }
    is_there_sd = true;


    // Step 1: Generate unique test string with random number
    rand_num = rand();
    // Use snprintf to prevent buffer overflow (limit to buffer size - 1 for null terminator)
    snprintf(save_str, sizeof(save_str) - 1, "SD CARD TEST RAND NUM  %d", rand_num);
    write_len = strlen(save_str);
    ESP_LOGI(TAG, "Test string to write: %s (length: %zu)", save_str, write_len);

    // Step 2: Open file for writing (w+: create if not exists, truncate if exists, read/write mode)
    write_fp = fopen(RECORD_FILENAME, "w+");
    if (write_fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", RECORD_FILENAME);
        goto cleanup;
    }

    // Step 3: Write content to file (fixed parameter order: ptr, size_per_element, element_count, file_ptr)
    actual_written = fwrite(save_str, 1, write_len, write_fp);
    if (actual_written != write_len) {
        ESP_LOGE(TAG, "Write failed: expected %zu bytes, actual %zu bytes", write_len, actual_written);
        goto cleanup;
    }

    // Step 4: Ensure data is flushed to physical SD card (critical for reliable read-back)
    // fflush: Flush user-space C library cache to filesystem cache
    if (fflush(write_fp) != 0) {
        ESP_LOGE(TAG, "fflush failed (user-space cache not flushed)");
        goto cleanup;
    }
    // fsync: Force filesystem to write cache to physical SD card (catches hardware-level errors)
    if (fsync(fileno(write_fp)) != 0) {
        ESP_LOGE(TAG, "fsync failed (data not written to physical SD card)");
        goto cleanup;
    }

    // Step 5: Close write file pointer (release resource)
    fclose(write_fp);
    write_fp = NULL;  // Mark as closed to avoid double-close

    // Short delay (optional but safe: ensures SD card completes physical write)
    vTaskDelay(pdMS_TO_TICKS(10));

    // Step 6: Open file for reading (r: read-only mode)
    read_fp = fopen(RECORD_FILENAME, "r");
    if (read_fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", RECORD_FILENAME);
        goto cleanup;
    }

    // Step 7: Read content from file (limit to read_buf size - 1 to reserve null terminator)
    actual_read = fread(read_buf, 1, sizeof(read_buf) - 1, read_fp);
    read_buf[actual_read] = '\0';  // Manually add null terminator (fread doesn't include it)
    ESP_LOGI(TAG, "Read content: %s (length: %zu)", read_buf, actual_read);

    // Step 8: Verify read content matches written content
    if (actual_read == write_len && strcmp(read_buf, save_str) == 0) {
        ESP_LOGI(TAG, "SD card read/write test PASSED");
        test_success = true;
    } else {
        ESP_LOGE(TAG, "Test FAILED: content mismatch (written: %s, read: %s)", save_str, read_buf);
    }

cleanup:
    // Ensure all file pointers are closed to prevent resource leaks
    if (write_fp != NULL) {
        fclose(write_fp);
    }
    if (read_fp != NULL) {
        fclose(read_fp);
    }

    // Step 9: Update LVGL UI (thread-safe with lock/unlock)
    if (lvgl_port_lock(0)) {  // Acquire LVGL lock to avoid UI conflicts
        lv_obj_clear_flag(result_label,LV_OBJ_FLAG_HIDDEN);

        if(is_there_sd)
        {
            if (test_success) {
            lv_label_set_text(result_label, "#00ff00  WRITE SUCCESS #");
            } else {
                lv_label_set_text(result_label, "#ff0000  WRITE ERROR #");
            }
        }
        else
        {
            lv_label_set_text(result_label, "#ff0000  NO SD CARD DETECTED #");
        }
        lvgl_port_unlock();  // Release LVGL lock
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

    lcd_brightness_init();                    // Initialize LCD brightness 
    lcd_brightness_set(100);                  // Set LCD brightness to maximum brightness (100%)
    

    esp_sdcard_port_init();  


    /**
     * Initialize LVGL file system interface (provides basic file operation support for LVGL components)
     * This function sets up the underlying file system integration required by LVGL (e.g., for loading images/fonts from storage)
     */
    lv_fs_if_init();

    /**
     * Attempt to acquire LVGL port lock to ensure thread-safe access to LVGL UI components
     * The parameter '0' indicates no timeout (returns true immediately if lock is available)
     */
    if (lvgl_port_lock(0))
    {
        // Create a button object, parented to the current active screen (lv_scr_act())
        bnt = lv_btn_create(lv_scr_act());       
        // Align the button to the center of the screen (x: 0 offset, y: 0 offset)
        lv_obj_align(bnt, LV_ALIGN_CENTER, 0, 30);
        
        // Set button background color to blue (0x2195f6) for its main part in default state
        lv_obj_set_style_bg_color(bnt, lv_color_hex(0x2195f6), LV_PART_MAIN | LV_STATE_DEFAULT);
        // Set button corner radius to 20 pixels (rounded corners) for its main part in default state
        lv_obj_set_style_radius(bnt, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
        // Disable shadow for the button's main part in default state (shadow width = 0)
        lv_obj_set_style_shadow_width(bnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        // Disable border for the button's main part in default state (border width = 0)
        lv_obj_set_style_border_width(bnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        // Add a click event callback to the button: trigger 'bnt_event_cb' when the button is clicked
        lv_obj_add_event_cb(bnt, bnt_event_cb, LV_EVENT_CLICKED, NULL);
    
        // Create a label object as a child of the button (text will be displayed on the button)
        bnt_label = lv_label_create(bnt);
        // Enable text recoloring for the label (allows using #color codes in text)
        lv_label_set_recolor(bnt_label, true);   
        // Set initial label text with white color (#ffffffff) and display "SD CARD TEST"
        lv_label_set_text(bnt_label, "#ffffffff  SD CARD TEST #"); 
        

        result_label = lv_label_create(lv_scr_act());

        lv_obj_align(result_label,LV_ALIGN_CENTER,0,-30);

        // Enable text recoloring for the label (allows using #color codes in text)
        lv_label_set_recolor(result_label, true);  

        lv_obj_add_flag(result_label,LV_OBJ_FLAG_HIDDEN);

        // Release the LVGL port lock to allow other threads/tasks to access LVGL components
        lvgl_port_unlock();
    }
   
}