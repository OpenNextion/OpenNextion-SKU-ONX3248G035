#ifndef _BOARD_INCLUDE_BOARD_EXIO_H_
#define _BOARD_INCLUDE_BOARD_EXIO_H_

#include "board.h"
#include "esp_err.h"


#define PCF8574_IIC_SPEED    100000 
#define PCF8574_ADDR         0x38


typedef enum  
{
    I2S_CTRL = EXPAND_IO_I2S_CTRL,
    RTC_INT = EXPAND_IO_RTC_INT,
    CHG_N = EXPAND_IO_CHG_N,
    CAM_PWDN = EXPAND_IO_CAM_PWDN,
    TP_INT = EXPAND_IO_TP_INT, 
    LCD_RST = EXPAND_IO_LCD_RST, 
    SDCS = EXPAND_IO_SDCS
}ExpandIoPin;
  

typedef enum  
{
    EXPAND_IO_LEVEL_L = 0,
    EXPAND_IO_LEVEL_H, 
    EXPAND_IO_LEVEL_ERR
}ExpandIoLevel;



/**
 * 初始化函数
 */
esp_err_t expandIoInit(void);

/**
 * 设置拓展IO引脚输出电平
 * io    :为 ExpandIoPin 的一员
 * level :为 ExpandIoLevel 的一员 高或者低电平
 */
esp_err_t expandIoControlIo(ExpandIoPin io,ExpandIoLevel level);

/**
 * 读取拓展IO上引脚的电平
 * io     :为 ExpandIoPin 的一员
 * return :为 ExpandIoLevel 的一员  高或者低电平
 */
ExpandIoLevel expandIoReadIo(ExpandIoPin io);


#endif /*_NEXTIOM_PROJECT_MODULE_EXPAND_IO_EXPAND_IO_H_ */ 
