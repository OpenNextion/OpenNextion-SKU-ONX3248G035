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

static const uint8_t PCF8574_PIN_CHG_N = 2;
static const uint8_t PCF8574_PIN_LCD_RST = 6;

static const uint8_t BATTERY_ADC_PIN = 4;
static const uint32_t BATTERY_RESISTOR_UPPER = 1050000;
static const uint32_t BATTERY_RESISTOR_LOWER = 2150000;
