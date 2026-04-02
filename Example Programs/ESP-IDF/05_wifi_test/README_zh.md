# ESP32-S3 Wi-Fi 测试示例

## 概述

本示例演示在 ONX3248G035 开发板（ESP32-S3）上进行 Wi-Fi 扫描、连接与状态显示。界面基于 LVGL，配网逻辑由 `wifi_manager` 组件实现。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **Wi-Fi 扫描**：扫描附近热点列表。
- **Wi-Fi 连接**：选择热点并连接（支持常见 WPA/WPA2）。
- **状态显示**：显示连接状态与错误提示。

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

1. **连接硬件**：连接 LCD、触摸与开发板。
2. **编译烧录**：完成编译并烧录。
3. **开始配网**：启动后在屏幕上扫描并选择热点，输入密码后连接。

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

- Wi-Fi 仅支持 2.4 GHz（ESP32-S3 限制）。
- 如需修改默认配置，请通过 `menuconfig` 调整 Wi-Fi 相关选项。

