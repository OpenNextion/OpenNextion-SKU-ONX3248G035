#ifndef _BOARD_INCLUDE_BOARD_TOUCH_H_
#define _BOARD_INCLUDE_BOARD_TOUCH_H_

#include "esp_lcd_touch.h"
#include "board_lvgl.h"

esp_lcd_touch_handle_t xLcdTouchGetHandle(void);
void vlcdTouchInit( uint16_t xmax, uint16_t ymax);
void touch_test(void);


#endif /*_BOARD_INCLUDE_BOARD_TOUCH_H_*/