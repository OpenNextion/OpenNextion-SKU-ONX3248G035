# ESP32-S3 Arduino Battery Test Example

## Overview

This is the ESP-Arduino version of `08_battery_test`. It tests battery-voltage sampling, estimated capacity display, and charge-state display on the ONX3248G035 board.

Libraries used:

- LCD: `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- UI: `lvgl`
- ADC: ESP32 Arduino core ADC API

## Features

- Reads the divided battery voltage from GPIO4.
- Converts the ADC voltage with the same divider values used by the ESP-IDF example.
- Reads charging state from the PCF8574 `CHG_N` pin.
- Shows battery icon, percentage, voltage, and charging state with LVGL.

## Capacity Display Strategy

The ESP-IDF version uses the `espressif/adc_battery_estimation` component and an OCV-SOC model. Arduino IDE cannot directly consume ESP-IDF Component Manager components, so this example keeps a lightweight Arduino implementation:

- Each ADC update averages `16` millivolt readings.
- Battery voltage is restored with `Vbat = Vadc * (Rupper + Rlower) / Rlower`.
- Voltage is low-pass filtered with `filtered = filtered * 7/8 + new * 1/8`.
- Raw capacity is linearly estimated from `3300 mV -> 0%` and `4200 mV -> 100%`.
- Displayed percentage uses `2%` hysteresis to avoid 97% / 98% boundary flicker.
- When USB power is plugged or unplugged, the displayed percentage is held first, then changes by at most `1%` every `30 s` to avoid charge-voltage jumps.

The voltage line shows filtered terminal voltage; the percentage shows stabilized estimated capacity.

## Arduino IDE Settings

When opening this example in Arduino IDE, use:

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## Files

- `08_battery_test.ino`: Main Arduino sketch.
- `BoardConfig.h`: LCD, touch, PCF8574, ADC, and resistor-divider configuration.
- `build_opt.h`: LVGL build option; it currently skips the default `lv_conf.h`.
