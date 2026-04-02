#ifndef _BOARD_INCLUDE_BOARD_BOARD_H_
#define _BOARD_INCLUDE_BOARD_BOARD_H_


#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_err.h"

#define NEXTION_VER "1.1.0"
/* *****************************  EXPAND_IO ****************************** */
#define EXPAND_IO_CH422    (0)
#define EXPAND_IO_PCF8574  (1)

#if EXPAND_IO_CH422
    #define EXPAND_IO_LCD_RST   (1)
    #define EXPAND_IO_TP_INT    (2)
    #define EXPAND_IO_LCD_BL    (3)
    #define EXPAND_IO_SDCS      (4)
    #define EXPAND_IO_I2S_CTRL  (5) 
    #define EXPAND_IO_RTC_INT   (6)
    #define EXPAND_IO_CAM_PWDN  (7)
#endif

#if EXPAND_IO_PCF8574
    #define EXPAND_IO_I2S_CTRL  (1) 
    #define EXPAND_IO_RTC_INT   (2)
    #define EXPAND_IO_CHG_N     (3)
    #define EXPAND_IO_CAM_PWDN  (4)
    #define EXPAND_IO_TP_INT    (5)
    #define EXPAND_IO_LCD_RST   (6)
    #define EXPAND_IO_SDCS      (7)
#endif


/* *************************************  lcd ********************************** */
#define  NEXTION_3_5  (1)
#define  NEXTION_2_8  (0)


#if NEXTION_3_5
    #define LCD_WIDTH   ((uint16_t)480)
    #define LCD_HEIGHT  ((uint16_t)320)
    #define NEXTION_FRIMWARE_NAME "FWLCD-ONXG035-NEXTION-ESP32S3"

    #define LCD_MODEL_ST7796U  (1)
    #define LCD_MODEL_ST77922  (0)
#endif

#if NEXTION_2_8
    #define LCD_WIDTH   ((uint16_t)320)
    #define LCD_HEIGHT  ((uint16_t)240)
    #define NEXTION_FRIMWARE_NAME "FWLCD-ONXG028-NEXTION-ESP32S3"

    #define LCD_MODEL_GC9307   (0)
    #define LCD_MODEL_ST7789   (1)
#endif

    #define LCD_HOST               SPI2_HOST

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
    #define LCD_BL_LEDC_DUTY_RES   LEDC_TIMER_10_BIT // Set duty resolution to 13 bits
    #define LCD_BL_LEDC_DUTY       (1024)            // Set duty to 50%. (2 ** 13) * 50% = 4096
    #define LCD_BL_LEDC_FREQUENCY  (10000)           // Frequency in Hertz. Set frequency at 5 kHz


/* *************************************  CAMERA ********************************** */

#define NEXTION_IIC_SCL_PIN  7
#define NEXTION_IIC_SDA_PIN  8

void boardIIcAddDevToNum0(void);
void boardIIcAddDevToNum1(void);

esp_err_t boardSpiInit(spi_host_device_t SPI_ID,
                      gpio_num_t miso_pin,
                      gpio_num_t mosi_pin,
                      gpio_num_t sclk_pin,
                      int clk_hz);
                      
  
#endif /* _BOARD_INCLUDE_BOARD_BOARD_H_  */
