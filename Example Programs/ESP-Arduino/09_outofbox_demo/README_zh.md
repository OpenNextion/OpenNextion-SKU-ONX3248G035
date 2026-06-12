# ESP32-S3 Arduino 开箱演示示例

## 概述

本示例是 `09_outofbox_demo` 的 ESP-Arduino 版本，用于运行 ONX3248G035 的开箱 LVGL 综合演示界面。

Arduino 版本已移植 ESP-IDF 例程中的自定义 `lv_demo_widgets` 源码和图片资源，界面效果与 ESP-IDF 版本保持一致。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- UI：`lvgl` 8.3.x

## 功能

- 使用 LVGL widgets demo 显示综合控件演示界面。
- 提供 `Profile`、`Analytics`、`Shop` 三个 Tab 页面。
- `Profile` 页面显示 Open Nextion 头像、说明、邮箱和电话信息。
- `Analytics` 页面显示 meter、chart 等 LVGL 控件效果。
- `Shop` 页面显示商品图片、列表和统计图表。
- 支持触摸滑动、Tab 切换、文本框、下拉框、日历和颜色主题切换。

## 文件

- `09_outofbox_demo.ino`：Arduino 主程序，负责 LCD、触摸、PCF8574 和 LVGL 初始化。
- `BoardConfig.h`：ONX3248G035 的引脚和 I2C 地址配置。
- `build_opt.h`：LVGL 和板型编译宏，包含 `NEXTION_3_5=1`。
- `src/widgets/lv_demo_widgets.c`：从 ESP-IDF 例程移植的自定义 widgets demo。
- `src/widgets/assets/`：ESP-IDF 例程使用的图片资源 C 文件。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

这些配置也可以通过 `arduino-cli` 的 FQBN 参数指定。

## 说明

本例程没有重新实现 UI，而是复用并移植 ESP-IDF 例程中的 LVGL widgets demo。Arduino 主程序只负责标准库驱动初始化和 LVGL 显示/触摸适配。
