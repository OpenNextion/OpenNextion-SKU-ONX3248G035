# ESP32-S3 Arduino Touch Test Example

## Overview

This is the ESP-Arduino version of `01_touch_test` for the ONX3248G035 3.5-inch display board. It uses standard Arduino libraries for the hardware drivers and LVGL for UI rendering:

- LCD: `Arduino_ST7796` from `GFX Library for Arduino`
- Touch: `Adafruit CST8XX Library`
- IO expander: `Adafruit PCF8574`
- UI: `lvgl`

The screen shows a white background and a gray `Touch Test` label. When the panel is touched, the sketch draws a red 4x4 pixel square at the touch position and prints the touch coordinates to the serial monitor.

LVGL renders the UI text. The touch trail follows the ESP-IDF example approach: it draws 4x4 red blocks directly through the LCD driver, avoiding unbounded LVGL object creation during long touch tests.

## Files

- `01_touch_test.ino`: Main sketch, directly openable in Arduino IDE.
- `BoardConfig.h`: Board pins, resolution, and I2C addresses.
- `build_opt.h`: Arduino ESP32 build options used to make LVGL use its built-in default configuration.

## Hardware Pins

| Function | ESP32-S3 Pin |
| :--- | :--- |
| I2C SCL | GPIO7 |
| I2C SDA | GPIO8 |
| LCD SCLK | GPIO5 |
| LCD MOSI | GPIO1 |
| LCD DC | GPIO3 |
| LCD CS | GPIO2 |
| LCD BL | GPIO6 |
| LCD RST | PCF8574 P6 |

## Arduino IDE Setup

1. Install the ESP32 Arduino core.
2. Install these libraries: `GFX Library for Arduino`, `Adafruit CST8XX Library`, `Adafruit PCF8574`, and `lvgl`.
3. Open `ESP-Arduino/01_touch_test/01_touch_test.ino`.
4. Select an ESP32-S3 board.
5. Recommended settings:
   - USB CDC On Boot: Enabled
   - PSRAM: Enabled
   - Flash Size: select the option that matches the module
6. Compile and upload.
7. Open the serial monitor at 115200 baud to view touch coordinates.

## Display Direction

The display direction can be configured at the top of `01_touch_test.ino`:

```cpp
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
```

The default values match the ESP-IDF example direction configuration.

## Implementation Notes

- LCD reset is controlled by PCF8574 P6. Other expander pins are kept high during reset to avoid disturbing shared peripherals.
- LVGL renders the initial UI and label. Root-screen scrolling is disabled so edge touches do not drag the page.
- Touch dots are drawn directly with `Arduino_GFX`, similar to the ESP-IDF example's `esp_lcd_panel_draw_bitmap()` path.
