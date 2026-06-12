#pragma once

#include <Arduino.h>

static const uint8_t NEXTION_IIC_SCL_PIN = 7;
static const uint8_t NEXTION_IIC_SDA_PIN = 8;

static const uint16_t LCD_H_RES = 320;
static const uint16_t LCD_V_RES = 480;

static const int8_t LCD_SCLK_PIN = 5;
static const int8_t LCD_MOSI_PIN = 1;
static const int8_t LCD_MISO_PIN = -1;
static const uint8_t LCD_DC_PIN = 3;
static const uint8_t LCD_CS_PIN = 2;
static const uint8_t LCD_BL_PIN = 6;

static const uint8_t CST826_ADDR = 0x15;
static const uint8_t PCF8574_ADDR = 0x38;

static const uint8_t PCF8574_PIN_CAM_PWDN = 3;
static const uint8_t PCF8574_PIN_LCD_RST = 6;

static const int8_t CAM_PWDN_GPIO = -1;
static const int8_t CAM_RESET_GPIO = -1;
static const uint8_t CAM_XCLK_GPIO = 38;
static const uint8_t CAM_SIOD_GPIO = 8;
static const uint8_t CAM_SIOC_GPIO = 7;
static const uint8_t CAM_Y9_GPIO = 21;
static const uint8_t CAM_Y8_GPIO = 39;
static const uint8_t CAM_Y7_GPIO = 40;
static const uint8_t CAM_Y6_GPIO = 42;
static const uint8_t CAM_Y5_GPIO = 46;
static const uint8_t CAM_Y4_GPIO = 48;
static const uint8_t CAM_Y3_GPIO = 47;
static const uint8_t CAM_Y2_GPIO = 45;
static const uint8_t CAM_VSYNC_GPIO = 17;
static const uint8_t CAM_HREF_GPIO = 18;
static const uint8_t CAM_PCLK_GPIO = 41;
