# ESP32-S3 Arduino Camera Test Example

## Overview

This is the ESP-Arduino version of `06_camera_test`. It tests camera capture and live LCD preview on the ONX3248G035 board.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- UI: `lvgl`
- Camera: `esp_camera` from the ESP32 Arduino core

## Features

- ST7796U display through Arduino_GFX.
- CST826 touch through Adafruit CST8XX.
- LVGL title and status label.
- RGB565/QVGA camera capture through the ESP32 Arduino core camera driver.
- Camera frames are pushed directly to the LCD preview area with Arduino_GFX to reduce per-frame copies.

## Runtime Flow

1. Initialize I2C, PCF8574, LCD, backlight, and touch.
2. Pull camera `PWDN` low through PCF8574 to wake the camera module.
3. Initialize `esp_camera` with QVGA `320x240` and `RGB565` output.
4. Draw the `Camera Test` title and status text with LVGL.
5. In the main loop, capture one frame about every 80 ms and draw it in the middle of the screen.

## Camera Configuration

- Frame size: `FRAMESIZE_QVGA`
- Resolution: `320x240`
- Pixel format: `PIXFORMAT_RGB565`
- XCLK: `20 MHz`
- Uses two frame buffers when PSRAM is available; otherwise falls back to one DRAM frame buffer.

This example matches the ESP-IDF behavior: after boot, it continuously displays the camera preview and does not provide a pause/resume button.

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## Files

- `06_camera_test.ino`: Main Arduino sketch.
- `BoardConfig.h`: LCD, touch, PCF8574, and camera pin configuration.
- `build_opt.h`: LVGL build option; it currently skips the default `lv_conf.h`.
