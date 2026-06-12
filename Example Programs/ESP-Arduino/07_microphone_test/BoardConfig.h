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

static const uint8_t PCF8574_PIN_I2S_CTRL = 0;
static const uint8_t PCF8574_PIN_LCD_RST = 6;

static const uint8_t PDM_CLK_PIN = 19;
static const uint8_t PDM_DATA_PIN = 20;

static const uint8_t SPEAKER_BCLK_PIN = 14;
static const uint8_t SPEAKER_DATA_PIN = 15;
static const uint8_t SPEAKER_WS_PIN = 16;
