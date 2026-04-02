# ESP32-S3 电池电量测试示例

## 概述

本示例演示在 ONX3248G035 开发板（ESP32-S3）上通过 ADC 采样电池电压，并在 LCD 上显示电量百分比与充电状态图标。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **电压采样**：ADC 采样电池分压后的电压。
- **电量估算**：基于电压估算电池百分比。
- **充电状态显示**：读取充电状态引脚并显示图标。
- **触摸 UI**：LVGL 显示电池状态。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **LCD 屏幕**：3.5 英寸 ST7796U SPI TFT LCD（分辨率：320x480）
- **触摸控制器**：CST826 I2C 电容触摸控制器
- **IO 扩展器**：PCF8574 I2C IO 扩展器
- **电池电压分压**：与原理图一致的分压电阻

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
| **电池电压 ADC** | | |
| ADC 通道 | ADC_CHANNEL_3 (GPIO4) | 电池分压采样 |
| **其他** | | |
| LCD BL | GPIO_NUM_6 | 背光控制（PWM） |
| LCD RST | - | 通过 PCF8574 控制 |

## 关键参数

- 分压电阻（参考原理图，可适当调整）：
  - 上拉：1.05M（`TEST_RESISTOR_UPPER`）
  - 下拉：2.15M（`TEST_RESISTOR_LOWER`）

## 使用说明

1. **连接硬件**：确认电池分压电阻与 ADC 引脚接线正确。
2. **编译烧录**：完成编译并烧录。
3. **查看显示**：屏幕右上角显示电池图标与百分比/充电状态。

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

## 备注

- 充电状态引脚通过 PCF8574 读取，代码中假定 `CHG_N` 高电平表示正在充电。

