#ifndef  _BOARD_INCLUDE_BOARD_LVGL_H_
#define  _BOARD_INCLUDE_BOARD_LVGL_H_


#include "board_lcd.h"
#include "board_touch.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#define EXAMPLE_LCD_H_RES  LCD_HEIGHT
#define EXAMPLE_LCD_V_RES  LCD_WIDTH

#define LCD_BUFFER_SIZE EXAMPLE_LCD_H_RES *EXAMPLE_LCD_V_RES / 8

void lv_port_init(void);


#endif /* _BOARD_INCLUDE_BOARD_LVGL_H_ */

