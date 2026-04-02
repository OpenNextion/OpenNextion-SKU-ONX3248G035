# ESP32-S3 麦克风测试示例

## 概述

本示例演示在 ONX3248G035 开发板（ESP32-S3）上进行麦克风采集、语音前端处理（AFE）以及回放的完整流程。界面基于 LVGL，按住 MIC 按钮录音，松开后循环播放，STOP 按钮停止录音/播放。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)
- 推荐麦克风配件：[Nextion Dual MIC Board](https://itead.cc/product/nextion-dual-mic-board/)
- 推荐音频配件：[Nextion BOX Speaker](https://itead.cc/product/nextion-box-speaker/)

## 功能

- **麦克风采集**：PDM 麦克风输入采集。
- **语音前端处理**：启用 NS/VAD/AGC/SE（视模型可用性）。
- **回放**：录音保存在 RAM 中并循环播放。
- **触摸 UI**：LVGL 界面控制录音/播放。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **LCD 屏幕**：3.5 英寸 ST7796U SPI TFT LCD（分辨率：320x480）
- **触摸控制器**：CST826 I2C 电容触摸控制器
- **IO 扩展器**：PCF8574 I2C IO 扩展器
- **麦克风**：需外接 PDM 麦克风
- **喇叭**：需外接喇叭/功放
- **PSRAM**：建议开启（录音缓存）

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

> 麦克风与喇叭相关引脚由 `board_mic` / `board_speaker` 组件配置。

## 使用说明

1. **连接硬件**：确保麦克风/喇叭/LCD 正常。
2. **编译烧录**：完成编译并烧录。
3. **录音**：长按 MIC 按钮开始录音。
4. **播放**：松开 MIC 按钮后开始循环播放。
5. **停止**：点击 STOP 按钮停止录音和播放。

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

- 该示例依赖 AFE 模型（`esp_afe_sr_models`），模型需位于 `model` 分区。
- PSRAM 不足时可能无法完成录音缓存或 AFE 初始化。

