#include "lcd_st7796u.h"
#include "board_lcd.h"
#include "esp_log.h"



static esp_lcd_panel_handle_t      panel_handle = NULL;
static esp_lcd_panel_io_handle_t   lcd_io_handle = NULL;     /* LCD IO设备句柄 */

static char *TAG = "spi_lcd";

esp_lcd_panel_handle_t spiLcdGetPanelHandle(void)
{
    return panel_handle;
}

esp_lcd_panel_io_handle_t spiLcdGetLcdIoHandle(void)
{
    return lcd_io_handle;
}


static void test_draw_color_bar(esp_lcd_panel_handle_t panel_handle, uint16_t h_res, uint16_t v_res)
{
 
    uint8_t byte_per_pixel = (16 + 7) / 8;
    uint16_t row_line = v_res / byte_per_pixel / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * h_res * byte_per_pixel, MALLOC_CAP_DMA);

    for (int j = 0; j < byte_per_pixel * 8; j++) {
        for (int i = 0; i < row_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), 16) >> (k * 8)) & 0xff;
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, h_res, (j + 1) * row_line, color));
      
    }

    uint16_t color_line = row_line * byte_per_pixel * 8;
    uint16_t res_line = v_res - color_line;
    if (res_line) {
        for (int i = 0; i < res_line * h_res; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = 0xff;
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, color_line, h_res, v_res, color));
       
    }

    free(color);
}



uint16_t buffer[490];

void spilcdClear(uint16_t color)
{
    /* 以40行作为缓冲,提高速率,若出现内存不足,可以减少缓冲行数 */
    //uint16_t *buffer = heap_caps_malloc(LCD_WIDTH * sizeof(uint16_t) , MALLOC_CAP_DMA);

    printf("LCD_WIDTH = %d\r\n",LCD_WIDTH );
    for (uint32_t i = 0; i < 481 ; i++)
    {
        buffer[i] = (color);
    }

    for (uint16_t y = 0; y < 480; y+=1)
    {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, 320, y+1, buffer);
    }
}



/**
 * @brief       spilcd初始化
 * @param       无
 * @retval      ESP_OK:初始化成功
 */
esp_err_t spiLcdInit(void)
{
    #if LCD_MODEL_ST7796U
        //一定记得要复位
        CTL_LCD_RST(0);
        vTaskDelay(pdMS_TO_TICKS(200));   
        CTL_LCD_RST(1);
        vTaskDelay(pdMS_TO_TICKS(200));   

        ESP_ERROR_CHECK(boardSpiInit(LCD_HOST,-1,LCD_MOSI_PIN,LCD_SCLK_PIN,LCD_WIDTH*LCD_HEIGHT/ 8));

        /* spi配置 */
        esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num         = LCD_DC_PIN,          /* DC IO */
            .cs_gpio_num         = LCD_CS_PIN,          /* CS IO */
            .pclk_hz             = 80 * 1000 * 1000,    /* PCLK为60MHz */
            .lcd_cmd_bits        = 8,                   /* 命令位宽 */
            .lcd_param_bits      = 8,                   /* LCD参数位宽 */
            .spi_mode            = 0,                   /* SPI模式 */
            .trans_queue_depth   = 5,                   /* 传输队列 */
            .on_color_trans_done = NULL,
            .user_ctx = NULL,

        };
        /* 将LCD设备挂载至SPI总线上 */
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io_handle));

        /* LCD设备配置 */
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_RST_PIN,                  /* RTS IO */
            .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,    /* RGB颜色格式 */
            .bits_per_pixel = 16,                           /* 颜色深度 */
            
        };
        //.data_endian    = LCD_RGB_DATA_ENDIAN_BIG,    /* 大端顺序 */

        /* 为ST7789创建LCD面板句柄，并指定SPI IO设备句柄 */
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7796u(lcd_io_handle, &panel_config, &panel_handle));

        /* 复位LCD */
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        CTL_LCD_RST(0);
        vTaskDelay(pdMS_TO_TICKS(100));   
        CTL_LCD_RST(1);
        vTaskDelay(pdMS_TO_TICKS(100));   

        /* 初始化LCD句柄 */
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        /* 反显 */
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

        /* 打开屏幕 */
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
        
        spilcdClear(0x0000);        /* 清屏 */
        vTaskDelay(pdMS_TO_TICKS(50));   

    #endif

    #if LCD_MODEL_ST77922
        CTL_LCD_RST(1);
        ESP_LOGI(TAG, "Initialize SPI bus");
        const spi_bus_config_t buscfg = ST77922_PANEL_BUS_SPI_CONFIG(LCD_SCLK_PIN, LCD_MOSI_PIN, LCD_WIDTH*LCD_HEIGHT/ 8);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        ESP_LOGI(TAG, "Install panel IO");
        const esp_lcd_panel_io_spi_config_t io_config = ST77922_PANEL_IO_SPI_CONFIG(LCD_CS_PIN, LCD_DC_PIN,
                                                                                    NULL, NULL);
        // Attach the LCD to the SPI bus
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io_handle));

        ESP_LOGI(TAG, "Install LCD driver of st77922");

        const st77922_vendor_config_t vendor_config = {
            // .init_cmds = lcd_init_cmds,         // Uncomment these line if use custom initialization commands
            // .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st77922_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 0,
            },
        };

        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_RST_PIN,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_config,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_st77922(lcd_io_handle, &panel_config, &panel_handle));
        esp_lcd_panel_reset(panel_handle);

   
        CTL_LCD_RST(1);
        vTaskDelay(pdMS_TO_TICKS(120));

        
        esp_lcd_panel_init(panel_handle);
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
        esp_lcd_panel_disp_on_off(panel_handle, true);
        spilcdClear(0xF800);        /* 清屏 */
    #endif


    #if LCD_MODEL_ST7789
        CTL_LCD_RST(1);
        vTaskDelay(100);


        ESP_ERROR_CHECK(boardSpiInit(LCD_HOST,-1,LCD_MOSI_PIN,LCD_SCLK_PIN,LCD_WIDTH*LCD_HEIGHT/ 8));

        /* spi配置 */
        esp_lcd_panel_io_spi_config_t io_config = {
            .dc_gpio_num         = LCD_DC_PIN,          /* DC IO */
            .cs_gpio_num         = LCD_CS_PIN,          /* CS IO */
            .pclk_hz             = 80 *1000 * 1000 ,    /* PCLK为60MHz */
            .lcd_cmd_bits        = 8,                   /* 命令位宽 */
            .lcd_param_bits      = 8,                   /* LCD参数位宽 */
            .spi_mode            = 0,                   /* SPI模式 */
            .trans_queue_depth   = 5,                   /* 传输队列 */
            .on_color_trans_done =  NULL,
            .user_ctx = NULL,
        };
        /* 将LCD设备挂载至SPI总线上 */
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io_handle));


        /* LCD设备配置 */
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = LCD_RST_PIN,                  /* RTS IO */
            .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,    /* RGB颜色格式 */
            .bits_per_pixel = 16,                           /* 颜色深度 */
        };

        /* 为ST7789创建LCD面板句柄，并指定SPI IO设备句柄 */
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io_handle, &panel_config, &panel_handle));

 
        /* 复位LCD */
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

        // /* 初始化LCD句柄 */
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        // esp_err_t ret =  esp_lcd_panel_io_rx_param(lcd_io_handle,0x04,chip_id,4);
        // if (ret == ESP_OK) {
        //     // 打印读取到的原始字节
        //     ESP_LOGI(TAG, "Raw Chip ID1 bytes: 0x%02X 0x%02X 0x%02X 0x%02X",
        //             chip_id[0], chip_id[1], chip_id[2], chip_id[3]);

        //     // 根据数据手册解析ID。
        //     // 例如，ST7789的返回可能是 0x85 0x85 0x52，其中有用的部分是最后一个字节。
        //     // ILI9341的 0xD3 命令返回 3 字节：通常第一个字节是dummy，后两个是ID。
        //     uint32_t parsed_id = (chip_id[1] << 16) | (chip_id[2] << 8) | chip_id[3]; // 示例解析
        //     ESP_LOGI(TAG, "Parsed Chip ID: 0x%06X", parsed_id);
        // } else {
        //     ESP_LOGE(TAG, "Failed to read chip ID: 0x%X", ret);
        // }

        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);

        /* 反显 */
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

        /* 打开屏幕 */
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
            
        spilcdClear(0x0000);        /* 清屏 */
        vTaskDelay(pdMS_TO_TICKS(50));   
        
        //LCD_PWR(1);
    #endif
       

    return ESP_OK;   
}



void spiLcdBlinit(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LCD_BL_LEDC_MODE,
    ledc_timer.timer_num = LCD_BL_LEDC_TIMER;
    ledc_timer.duty_resolution = LCD_BL_LEDC_DUTY_RES;
    ledc_timer.freq_hz = LCD_BL_LEDC_FREQUENCY; // Set output frequency at 5 kHz
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = LCD_BL_LEDC_MODE;
    ledc_channel.channel = LCD_BL_LEDC_CHANNEL;
    ledc_channel.timer_sel = LCD_BL_LEDC_TIMER;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num = LCD_BL_PIN;
    ledc_channel.duty = 0, // Set duty to 0%
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}



void spiLcdSetBl(uint8_t intput_brightness)
{
    if (intput_brightness > 100)
    {
        intput_brightness = 100;
        ESP_LOGE(TAG, "intput_brightness value out of range");
    }
    uint32_t duty = (intput_brightness * (LCD_BL_LEDC_DUTY - 1)) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL));
    ESP_LOGI(TAG, "LCD brightness set to %d%%", intput_brightness);
}




