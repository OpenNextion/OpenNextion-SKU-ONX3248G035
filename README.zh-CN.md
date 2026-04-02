[English](./README.md) | [**简体中文**](./README.zh-CN.md)

# ONX3248G035

![ONX3248G035](<Product Images/ONX3248G035-1.jpg>)

## 项目简介

`ONX3248G035` 是一款基于 `ESP32-S3R8` 的 3.5 英寸 Open Nextion HMI 开发板，集成电容触摸显示、`2.4 GHz Wi-Fi`、`Bluetooth 5 LE`、`MicroSD`、摄像头、麦克风、喇叭、电池管理和多种扩展接口，适合用于人机交互界面、物联网终端、音视频交互和快速原型开发。

本仓库汇总了该型号的产品图片、尺寸图、原理图、PCB、认证资料、数据手册、USB 转串口驱动以及 `ESP-IDF` 示例工程，方便开发者下载、评估和二次开发。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)
- 可选音频配件：[Nextion BOX Speaker](https://itead.cc/product/nextion-box-speaker/)
- 可选麦克风配件：[Nextion Dual MIC Board](https://itead.cc/product/nextion-dual-mic-board/)
- 可选扩展配件：[Nextion IO Adapter V2](https://itead.cc/product/nextion-io-adapter-v2/)

## 硬件规格

以下参数整理自官方 Wiki 与仓库内配套资料：

| 项目 | 参数 |
| :--- | :--- |
| 型号 | `ONX3248G035` |
| 主控 | `ESP32-S3R8`，Xtensa LX7 双核，最高 `240 MHz` |
| Flash | `16 MB` |
| PSRAM | `8 MB` |
| 屏幕尺寸 | `3.5"` |
| 分辨率 | `320 x 480` |
| 显示色彩 | `262K` |
| 亮度 | `300 nit` |
| 触摸类型 | 电容触摸 |
| LCD 驱动 | `ST7796` |
| 触摸驱动 | `CST826` |
| 无线连接 | `2.4 GHz Wi-Fi` + `Bluetooth 5 LE` |
| 供电 | `DC 5V/1A` |
| 电池支持 | 支持 `3.7V` 锂电池接口与充电管理，支持 `3V` RTC 备份电池 |
| 主要扩展 | `USB-C`、Grove `I2C`、Grove `UART1`、`MicroSD`、摄像头接口、PDM 麦克风接口、喇叭接口、24Pin GPIO、外置天线接口 |
| 工作温度 | `-20 ~ 70 C` |
| 存储温度 | `-30 ~ 80 C` |
| 重量 | 净重约 `61 g`，毛重约 `115 g` |

## 主要特性

- 基于 `ESP32-S3`，适合做带图形界面的联网应用。
- 板载 `3.5"` 电容触摸屏，适合做控制面板、状态显示和交互终端。
- 提供 `MicroSD`、摄像头、`PDM` 麦克风、喇叭等扩展能力，便于做多媒体和语音交互项目。
- 支持 `USB-C` 供电、下载和串口日志输出，并支持自动下载模式，开发调试更方便。
- 板载天线并预留外置天线接口，适合无线通信场景。
- 支持锂电池供电与 RTC 备份电池，适合便携设备和离线时钟类应用。

## 仓库内容

| 目录 | 说明 |
| :--- | :--- |
| `Certifications/` | 认证与合规相关资料 |
| `Datasheets/` | 主要芯片与器件数据手册，如 `ESP32-S3`、`ST7796`、`CST826`、`PCF8574`、`IP4054V` |
| `Drawings/` | 结构尺寸图、`2D` 图纸、`3D` 模型 |
| `Example Programs/` | 示例工程，当前主要提供 `ESP-IDF` 例程 |
| `LTA Announcement/` | 官方相关公告文件 |
| `Product Images/` | 产品实物图片 |
| `Schematic & Layout /` | 原理图与 PCB 布局资料 |
| `USB-to-Serial Driver/` | USB 转串口驱动，含 Windows、macOS、Linux |

## 示例工程

`Example Programs/ESP-IDF/` 中已包含多个可直接参考的例程，仓库内大部分示例均附带中英文说明文档。

| 示例目录 | 说明 |
| :--- | :--- |
| `01_touch_test` | 触摸测试，验证 LCD 与触控功能 |
| `02_music_test` | 通过 `MicroSD` 播放音频并进行触摸交互 |
| `03_sd_card_image_test` | 从 `MicroSD` 读取并显示图片 |
| `04_sd_card_test` | `MicroSD` 挂载、读写与容量信息测试 |
| `05_wifi_test` | Wi-Fi 扫描、连接与状态显示 |
| `06_camera_test` | 外接 `OV2640` 摄像头并实时显示画面 |
| `07_microphone_test` | PDM 麦克风采集、语音前端处理与回放 |
| `08_battery_test` | 电池电压、电量估算和充电状态显示 |
| `09_outofbox_demo` | 出厂演示程序，快速验证 LCD、触摸和 LVGL UI |

## 快速上手

### 1. 安装驱动

如电脑未能识别串口设备，可先安装 `USB-to-Serial Driver/` 目录中的对应驱动：

- Windows: `CH340K-USB-to-UART-Driver.zip`
- macOS: `CH341SER_MAC.ZIP`
- Linux: `CH341SER_LINUX.zip`

### 2. 连接开发板

使用 `USB-C` 线连接开发板与电脑。`USB-C` 接口可用于：

- 供电
- 固件烧录
- 串口日志输出

### 3. 选择示例工程

进入 `Example Programs/ESP-IDF/`，选择需要的示例目录。根据示例自带的 `README_zh.md` 或 `README_en.md` 准备对应外设，例如 `MicroSD` 卡、摄像头、麦克风或喇叭。

### 4. 配置开发环境

仓库中的 `ESP-IDF` 示例工程说明显示：

- 开发版本：`ESP-IDF 5.4.1`
- 推荐版本：`ESP-IDF >= 5.4.0`

### 5. 编译与烧录

以出厂演示示例为例：

```bash
cd "Example Programs/ESP-IDF/09_outofbox_demo"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

如果首次下载失败，可按住 `BOOT` 键，再按一次 `RESET` 键进入下载模式后重新烧录。

## 使用注意事项

- `USB_UART` 与 `UART0` 的 `TX/RX` 为复用关系，不能同时使用。
- 24Pin GPIO 扩展口与 `UART1`、`MIC_PDM`、`CAMERA` 部分信号复用，使用时需注意资源冲突。
- `USB-C` 支持自动下载模式，而 `UART0` 不支持自动下载。
- Wi-Fi 仅支持 `2.4 GHz` 频段。
- 喇叭接口最大支持 `2W`。
- 如需使用外置天线，需要按官方硬件说明拆除对应电阻后切换天线路径。
- 开发板本体不防水，如有需要请搭配定制外壳使用。
- 详细尺寸请查看 `Drawings/ONX3248G035-Dimension.pdf`。
- 详细硬件电路请查看 `Schematic & Layout /` 中的原理图和 PCB 文件。

## 参考资料

- 官方产品页面：[ONX3248G035 Wiki](https://nextion.tech/wiki/onx3248g035/)
- ESP-IDF 入门文档：[ESP32-S3 Getting Started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)

如果你正在为该型号开发应用，建议先从 `09_outofbox_demo` 或 `01_touch_test` 开始，确认屏幕、触摸和基础环境正常后，再继续接入 `Wi-Fi`、`MicroSD`、摄像头或音频相关功能。
