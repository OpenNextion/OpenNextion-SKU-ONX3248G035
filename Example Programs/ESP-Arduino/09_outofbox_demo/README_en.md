# ESP32-S3 Arduino Out-of-Box Demo

## Overview

This is the ESP-Arduino version of `09_outofbox_demo`. It runs the ONX3248G035 out-of-box LVGL showcase.

The Arduino version ports the customized `lv_demo_widgets` source and image assets from the ESP-IDF example, so the UI matches the ESP-IDF version.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- UI: `lvgl` 8.3.x

## Features

- Shows the LVGL widgets demo as the main out-of-box UI.
- Provides the `Profile`, `Analytics`, and `Shop` tabs.
- The `Profile` tab shows the Open Nextion avatar, description, email, and phone number.
- The `Analytics` tab demonstrates meters, charts, and related LVGL widgets.
- The `Shop` tab shows product images, list items, and chart widgets.
- Supports touch scrolling, tab switching, text areas, dropdowns, calendar input, and theme-color switching.

## Files

- `09_outofbox_demo.ino`: Main Arduino sketch for LCD, touch, PCF8574, and LVGL initialization.
- `BoardConfig.h`: ONX3248G035 pin and I2C address configuration.
- `build_opt.h`: LVGL and board build macros, including `NEXTION_3_5=1`.
- `src/widgets/lv_demo_widgets.c`: Customized widgets demo ported from the ESP-IDF example.
- `src/widgets/assets/`: Image asset C files used by the ESP-IDF demo.

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

The same settings can also be supplied through the `arduino-cli` FQBN options.

## Notes

This example does not recreate the UI from scratch. It reuses the ESP-IDF example's LVGL widgets demo and keeps the Arduino sketch focused on standard-library driver initialization and LVGL display/touch bridging.
