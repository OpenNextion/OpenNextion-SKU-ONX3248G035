# ESP32-S3 Arduino SD Card Test Example

## Overview

This is the ESP-Arduino version of `04_sd_card_test`. It tests SD-card mounting, writing, reading, and verification.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- SD card: `SD_MMC` from the ESP32 Arduino core
- UI: `lvgl`

After tapping the `SD CARD TEST` button, the sketch re-detects and mounts the SD card, writes `/SD CARD TEST.txt` to the SD card root, reads it back, and compares the content. It shows `WRITE SUCCESS` when the readback matches, `NO SD CARD DETECTED` when no card is present, and an error message for other read/write failures.

## Test Flow

1. Initialize the LCD, CST826 touch controller, PCF8574 IO expander, and LVGL.
2. Tap the `SD CARD TEST` button.
3. Release the previous `SD_MMC` mount state and mount the SD card again.
4. Write a random string to `/SD CARD TEST.txt`.
5. Close the file, reopen it, and read the content back.
6. Compare the written and readback strings, then show the result on screen.

With this flow, removing the SD card after one successful test is handled on the next tap and shows `NO SD CARD DETECTED`.

## SD Card Notes

- The sketch uses `SD_MMC` from the ESP32 Arduino core in 1-bit mode.
- `/SD CARD TEST.txt` is created or overwritten in the SD card root.
- The file content is similar to `SD CARD TEST RAND NUM 12345678` and changes on every tap.

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## Files

- `04_sd_card_test.ino`: Main Arduino sketch.
- `BoardConfig.h`: Board pins, resolution, and SD-card pins.
- `build_opt.h`: LVGL build option; it currently skips the default `lv_conf.h`.
