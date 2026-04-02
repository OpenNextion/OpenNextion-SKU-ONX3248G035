

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "board_lvgl.h" 

static const char *TAG = "netion_lvgl";
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

static esp_lcd_panel_io_handle_t s_lcd_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;

static bool init_sign = false;



void lv_port_init(void)
{
    if(init_sign == true)
    {
        return;
    }
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&port_cfg);
    ESP_LOGI(TAG, "Adding LCD screen");

    s_panel_handle = spiLcdGetPanelHandle(); 
    s_lcd_handle = spiLcdGetLcdIoHandle();
    s_touch_handle  =  xLcdTouchGetHandle();

    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = s_lcd_handle,
        .panel_handle = s_panel_handle,
        .control_handle = NULL,
        .buffer_size = EXAMPLE_LCD_H_RES*EXAMPLE_LCD_V_RES/8,
        .double_buffer = true,
        .trans_size = 0,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
    
#if  NEXTION_3_5
        .rotation = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
#endif

#if  NEXTION_2_8
     .rotation = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
#endif

        .flags = {
            .buff_dma = 0,
            .buff_spiram = 1,
            .sw_rotate = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

#if EXAMPLE_DISPLAY_ROTATION == 90
    display_cfg.rotation.swap_xy = 1;
    display_cfg.rotation.mirror_x = 1;
    display_cfg.rotation.mirror_y = 1;
#elif EXAMPLE_DISPLAY_ROTATION == 180
    display_cfg.rotation.swap_xy = 0;
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 1;

#elif EXAMPLE_DISPLAY_ROTATION == 270
    display_cfg.rotation.swap_xy = 1; 
    display_cfg.rotation.mirror_x = 0;
    display_cfg.rotation.mirror_y = 0; 
#endif

    lvgl_disp = lvgl_port_add_disp(&display_cfg);
    if(lvgl_disp != NULL)
    {
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lvgl_disp,
            .handle = s_touch_handle,
        };
        lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
        init_sign = true;
    }
    else
    {
        printf("lv_port_init ERROR \r\n");
    }

}