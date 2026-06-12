# ESP32-S3 Arduino 音乐测试示例

## 概述

本示例是 `02_music_test` 的 ESP-Arduino 版本，用于 ONX3248G035 开发板的基础音乐文件测试。

程序使用标准 Arduino/ESP32 库和第三方库：

- LCD：`GFX Library for Arduino` 的 `Arduino_ST7796`
- 触摸：`Adafruit CST8XX Library`
- IO 扩展：`Adafruit PCF8574`
- SD 卡：ESP32 Arduino core 的 `SD_MMC`
- UI：`lvgl`，并移植 ESP-IDF 版本 `components/gui` 的 Music Demo 界面资源
- 音频播放：`ESP32-audioI2S-master`

示例会挂载 SD 卡，扫描 `/music` 目录中的 `.mp3` 文件，并使用与 ESP-IDF 版本同源的 LVGL Music Demo UI 显示封面、波形、曲目信息、播放控制和 ALL TRACKS 列表。点击曲目后会通过 I2S 输出到板载 NS4168 喇叭功放。

## 说明

当前 Arduino 版本已对齐 ESP-IDF 版本的 UI 效果，并使用成熟第三方库完成 MP3 解码和 I2S 播放，没有自写音频解码器。

## 音频与防爆破声

- I2S BCLK: GPIO14
- I2S LRCLK/WS: GPIO16
- I2S DOUT/SDATA: GPIO15
- NS4168 使能：PCF8574 `I2S_CTRL`

PCF8574 上电默认端口为高电平，若过早打开 `I2S_CTRL`，喇叭可能在 I2S 输出稳定前产生瞬间爆破声。本例程在初始化时保持 `I2S_CTRL=LOW`，只有在 MP3 解码和 I2S 输出启动后延迟打开功放；暂停或切歌时也会先关闭功放。

## ESP32S3R8 + 16MB Flash 配置

本例程移植了 ESP-IDF 版本的完整 LVGL Music UI 图片和字体资源，程序体积超过 Arduino 默认 `1.2MB APP` 分区。你的硬件为 ESP32S3R8 + 外挂 16MB Flash，请在 Arduino IDE 中选择：

- Board: `ESP32S3 Dev Module`
- Flash Size: `16MB (128Mb)`
- PSRAM: `OPI PSRAM`
- Partition Scheme: `16M Flash (3MB APP/9.9MB FATFS)`

工程内提供了 `sketch.yaml` 记录目标配置。当前 Arduino CLI 版本使用 `--profile` 时可能不加载本机已安装库路径，推荐使用已验证的完整 FQBN：

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi ESP-Arduino/02_music_test
arduino-cli upload --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi -p <你的串口> ESP-Arduino/02_music_test
```

## 文件

- `02_music_test.ino`: Arduino 主程序。
- `BoardConfig.h`: 板级引脚、分辨率、I2C 地址和 SD/I2S 引脚配置。
- `build_opt.h`: LVGL 编译选项。
- `sketch.yaml`: ESP32S3R8 + 16MB Flash 的目标配置记录。
- `src/gui`: 从 ESP-IDF 版本移植的 LVGL Music UI 源码和资源。
- `src/gui/gui_hal.cpp`: Arduino 适配层，负责 SD_MMC 扫描、MP3 播放、音量映射和功放时序控制。

## SD 卡目录

请在 SD 卡根目录创建：

```text
/music
```

并放入 `.mp3` 文件。

## 依赖库

请在 Arduino IDE 的 Library Manager 中安装：

- `GFX Library for Arduino`
- `Adafruit CST8XX Library`
- `Adafruit PCF8574`
- `lvgl` 8.3.x
- `ESP32-audioI2S-master`
