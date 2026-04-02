
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "board.h"
#include "board_touch.h"
#include "touch_cst826.h"



static esp_lcd_touch_handle_t touch_handle = NULL;

esp_lcd_touch_handle_t xLcdTouchGetHandle(void)
{
    return touch_handle;
}


void vlcdTouchInit( uint16_t xmax, uint16_t ymax)
{

    //uint16_t rotation = 0;
    i2c_master_bus_handle_t NUM_0_handle;

    esp_lcd_panel_io_handle_t touch_io_handle = NULL;
    
    esp_lcd_panel_io_i2c_config_t touch_io_config = {};
    touch_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_ADDRESS,
    touch_io_config.control_phase_bytes = 1;
    touch_io_config.dc_bit_offset = 0;
    touch_io_config.lcd_cmd_bits = 8;
    touch_io_config.flags.disable_control_phase = 1;
    touch_io_config.scl_speed_hz = 400 * 1000;

    ESP_ERROR_CHECK(i2c_master_get_bus_handle(0, &NUM_0_handle));

    if(NUM_0_handle != NULL)
    {
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(NUM_0_handle, &touch_io_config, &touch_io_handle));
    }

    /* Touch IO handle */
    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = xmax < ymax ? xmax : ymax;
    tp_cfg.y_max = xmax < ymax ? ymax : xmax;
    tp_cfg.rst_gpio_num = GPIO_NUM_NC;
    tp_cfg.int_gpio_num = GPIO_NUM_NC;

    
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;

    // if (90 == rotation)
    // {
    //     tp_cfg.flags.swap_xy = 1;
    //     tp_cfg.flags.mirror_x = 0;
    //     tp_cfg.flags.mirror_y = 1;
    // }
    // else if (180 == rotation)
    // {
    //     tp_cfg.flags.swap_xy = 0;
    //     tp_cfg.flags.mirror_x = 1;
    //     tp_cfg.flags.mirror_y = 1;
    // }
    // else if (270 == rotation)
    // {
    //     tp_cfg.flags.swap_xy = 1;
    //     tp_cfg.flags.mirror_x = 1;
    //     tp_cfg.flags.mirror_y = 0;
    // }

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst826(touch_io_handle, &tp_cfg, &touch_handle));
}




void touch_test(void)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;
    uint16_t color_arr[16] = {0};
    lv_obj_t *lable = NULL;
    static esp_lcd_touch_handle_t s_touch_handle;
    static esp_lcd_panel_handle_t s_panel_handle;
    s_panel_handle =  spiLcdGetPanelHandle();
    s_touch_handle = xLcdTouchGetHandle();
    for (int i = 0; i < 16; i++)
    {
        color_arr[i] = 0xf800;
    }
    if (lvgl_port_lock(0))
    {
        lable = lv_label_create(lv_scr_act());
        lv_label_set_text(lable, "NETION Touch testing");
        lv_obj_center(lable);
        lvgl_port_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    if (lvgl_port_lock(0))
    {
        while (1)
        {
            /* Read data from touch controller into memory */
           esp_lcd_touch_read_data(s_touch_handle);
            /* Read data from touch controller */
            bool touchpad_pressed = esp_lcd_touch_get_coordinates(s_touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);
            if (touchpad_pressed && touchpad_cnt > 0)
            {
                // touchpad_x[0] = EXAMPLE_LCD_H_RES - 1 - touchpad_x[0];
                if (touchpad_x[0] < 2)
                    touchpad_x[0] = 2;
                else if (touchpad_x[0] > EXAMPLE_LCD_H_RES - 2 - 1)
                    touchpad_x[0] = EXAMPLE_LCD_H_RES - 2 - 1;
                if (touchpad_y[0] < 2)
                    touchpad_y[0] = 2;
                else if (touchpad_y[0] > EXAMPLE_LCD_V_RES - 2 - 1)
                    touchpad_y[0] = EXAMPLE_LCD_V_RES - 2 - 1;
                esp_lcd_panel_draw_bitmap(s_panel_handle, touchpad_x[0] - 2, touchpad_y[0] - 2, touchpad_x[0] + 2, touchpad_y[0] + 2, color_arr);
            }
            vTaskDelay(pdMS_TO_TICKS(2));
        }
        lv_obj_del(lable);
        lvgl_port_unlock();
    }
}





