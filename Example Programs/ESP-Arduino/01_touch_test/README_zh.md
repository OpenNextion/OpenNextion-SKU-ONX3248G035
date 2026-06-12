# ESP32-S3 Arduino 触摸测试示例

## 概述

本示例是 `01_touch_test` 的 ESP-Arduino 版本，用于 ONX3248G035 开发板的 3.5 寸屏基础触摸测试。程序使用 Arduino 标准库生态完成硬件驱动和 UI 绘制：

- LCD：`GFX Library for Arduino` 的 `Arduino_ST7796`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- UI：`lvgl`

屏幕显示白色背景和灰色 `Touch Test` 文本。触摸屏幕时，程序会在触点位置绘制红色 4x4 像素方块，并在串口监视器输出触摸坐标。

UI 文本由 LVGL 绘制；触摸轨迹参考 ESP-IDF 例程的实现方式，使用 LCD 驱动直接刷 4x4 红色小块，避免连续触摸时创建大量 LVGL 对象导致内存增长。

## 文件

- `01_touch_test.ino`: Arduino IDE 可直接打开的主程序。
- `BoardConfig.h`: 板级引脚、分辨率和 I2C 地址配置。
- `build_opt.h`: Arduino ESP32 编译选项，用于让 LVGL 使用内置默认配置。

## 硬件引脚

| 功能 | ESP32-S3 引脚 |
| :--- | :--- |
| I2C SCL | GPIO7 |
| I2C SDA | GPIO8 |
| LCD SCLK | GPIO5 |
| LCD MOSI | GPIO1 |
| LCD DC | GPIO3 |
| LCD CS | GPIO2 |
| LCD BL | GPIO6 |
| LCD RST | PCF8574 P6 |

## Arduino IDE 设置

1. 安装 ESP32 Arduino core。
2. 安装库：`GFX Library for Arduino`、`Adafruit CST8XX Library`、`Adafruit PCF8574`、`lvgl`。
3. 打开 `ESP-Arduino/01_touch_test/01_touch_test.ino`。
4. 开发板选择 ESP32-S3 类型的开发板。
5. 建议设置：
   - USB CDC On Boot: Enabled
   - PSRAM: Enabled
   - Flash Size: 按实际模组选项选择
6. 编译并烧录。
7. 打开 115200 波特率串口监视器查看触摸坐标。

## 屏幕方向

屏幕方向可在 `01_touch_test.ino` 顶部通过以下宏配置：

```cpp
#define DISPLAY_SWAP_XY false
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
```

当前默认值与 ESP-IDF 例程中的方向配置保持一致。

## 实现说明

- LCD 复位通过 PCF8574 P6 控制，初始化时保持其它扩展 IO 为高电平，避免影响共用扩展 IO 的外设。
- LVGL 负责初始界面和文字显示，根页面禁用滚动，避免触摸边缘时页面被拖动。
- 红点使用 `Arduino_GFX` 直接刷屏绘制，行为接近 ESP-IDF 例程中的 `esp_lcd_panel_draw_bitmap()`。
