# ESP32-S3 出厂演示示例

## 概述

本示例为 ONX3248G035 开发板（ESP32-S3）的出厂演示程序，基于 LVGL 运行官方 Widgets 示例，用于快速验证 LCD、触摸与 UI 渲染能力。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **LVGL Widgets 演示**：展示多种控件与交互效果。
- **触摸交互**：支持触摸操作与滚动。
- **LCD 显示**：ST7796U LCD 显示界面。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **LCD 屏幕**：3.5 英寸 ST7796U SPI TFT LCD（分辨率：320x480）
- **触摸控制器**：CST826 I2C 电容触摸控制器
- **IO 扩展器**：PCF8574 I2C IO 扩展器

## 引脚连接（与代码一致）

| 功能 | ESP32-S3 引脚 | 说明 |
| :--- | :--- | :--- |
| **I2C** | | |
| I2C SCL | GPIO_NUM_7 | I2C 时钟线 |
| I2C SDA | GPIO_NUM_8 | I2C 数据线 |
| **SPI (LCD)** | | |
| SPI SCLK | GPIO_NUM_5 | SPI 时钟线 |
| SPI MOSI | GPIO_NUM_1 | SPI 数据线 |
| LCD DC | GPIO_NUM_3 | LCD 数据/命令选择 |
| LCD CS | GPIO_NUM_2 | LCD 片选 |
| **其他** | | |
| LCD BL | GPIO_NUM_6 | 背光控制（PWM） |
| LCD RST | - | 通过 PCF8574 控制 |

## 使用说明

1. **连接硬件**：确认 LCD 与触摸连接正确。
2. **编译烧录**：完成编译并烧录。
3. **查看演示**：上电后自动进入 Widgets 演示界面。

## 编译与烧录

1. 设置目标芯片：
   ```bash
   idf.py set-target esp32s3
   ```
2. 编译：
   ```bash
   idf.py build
   ```
3. 烧录与监视：
   ```bash
   idf.py -p <PORT> flash monitor
   ```

