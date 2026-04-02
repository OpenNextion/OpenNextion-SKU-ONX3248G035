#ifndef _BOARD_INCLUDE_BOARD_LCD_TOUCH_CST826_H_
#define _BOARD_INCLUDE_BOARD_LCD_TOUCH_CST826_H_

#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_touch.h"

#define ESP_LCD_TOUCH_IO_I2C_ADDRESS     (0x15)
#define CONFIG_ESP_LCD_TOUCH_MAX_POINTS  (1)



esp_err_t esp_lcd_touch_new_i2c_cst826(const esp_lcd_panel_io_handle_t io, const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out_touch);

#endif /* _BOARD_INCLUDE_BOARD_LCD_TOUCH_CST826_H_ */



