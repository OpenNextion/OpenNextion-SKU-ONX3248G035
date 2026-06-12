# ESP32-S3 Arduino 麦克风录音回放示例

## 概述

本示例是 `07_microphone_test` 的 ESP-Arduino 版本，用于测试 ONX3248G035 的 PDM 麦克风录音和喇叭回放。

界面和 ESP-IDF 例程保持一致：长按 `MIC` 按钮录音，松开后循环播放刚才录到 RAM 中的音频，点击 `Stop` 停止录音或播放。

使用的库：

- LCD：`GFX Library for Arduino`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- UI：`lvgl`
- 麦克风：ESP32 Arduino core 自带 ESP-IDF `driver/i2s_pdm.h`
- 喇叭：ESP32 Arduino core 自带 ESP-IDF `driver/i2s_std.h`

## 运行流程

1. 初始化 LCD、CST826 触摸、PCF8574、PDM 麦克风和 I2S 喇叭输出。
2. 从 PSRAM 申请 30 秒录音缓存。
3. 长按 `MIC`：开始录音，按钮变红。
4. 松开 `MIC`：停止录音并开始循环回放，按钮变绿。
5. 点击 `Stop`：停止录音或回放，关闭喇叭功放。

## 音频配置

- 采样率：`16000 Hz`
- 位深：`16-bit`
- 录音格式：PDM RX 转 PCM mono
- 回放格式：I2S STD TX mono
- 最大录音时长：`30 s`
- 录音缓存：优先使用 PSRAM，失败时尝试内部 RAM

说明：ESP-IDF 版本中包含 ESP-SR/AFE 处理链路；Arduino 版本使用 ESP32 Arduino core 提供的标准 I2S 驱动完成录音与回放，不额外移植 ESP-SR/AFE 组件。

## Arduino IDE 设置

使用 Arduino IDE 打开本例程时，开发板建议选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

## 文件

- `07_microphone_test.ino`: Arduino 主程序。
- `BoardConfig.h`: LCD、触摸、PDM 麦克风、I2S 喇叭和 PCF8574 引脚配置。
- `build_opt.h`: LVGL 编译选项，当前仅跳过默认 `lv_conf.h`。
