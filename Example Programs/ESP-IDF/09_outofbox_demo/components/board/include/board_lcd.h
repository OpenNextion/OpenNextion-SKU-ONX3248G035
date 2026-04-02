#ifndef _BOARD_INCLUDE_BOARD_LCD_H_
#define _BOARD_INCLUDE_BOARD_LCD_H_


#include <unistd.h>
#include "driver/spi_common.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "board.h"

#include "board_exio.h"


#define CTL_LCD_RST(x)   do { x ?                                 \
                            expandIoControlIo(LCD_RST, EXPAND_IO_LEVEL_H):  \
                            expandIoControlIo(LCD_RST, EXPAND_IO_LEVEL_L);  \
                        } while(0)




esp_err_t spiLcdInit(void);
esp_lcd_panel_handle_t    spiLcdGetPanelHandle(void);
esp_lcd_panel_io_handle_t spiLcdGetLcdIoHandle(void);

//背光灯
void spiLcdBlinit(void);
void spiLcdSetBl(uint8_t intput_brightness);
void spilcdClear(uint16_t color);

#endif /* _BOARD_INCLUDE_BOARD_LCD_H_ */

