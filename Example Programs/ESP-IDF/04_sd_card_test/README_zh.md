# ESP32-S3 SD 卡测试示例

## 概述

本项目是一个在 ONX3248G035 开发板 (ESP32-S3) 上测试 SD 卡功能的示例。它演示了如何挂载 SD 卡、向其写入文件、读回文件以及检查卡的类型和容量等信息。

该示例在 1 线 SD 模式下使用 SDMMC 主机驱动程序。操作结果（文件写入、读取和卡信息）将打印到串行监视器。

## 购买链接

- 产品购买：[Open Nextion 3.5" Genius Series ESP32-S3 LCD Touchscreen Development Board](https://itead.cc/product/open-nextion-3-5-genius-series-esp32-s3-lcd-touchscreen-development-board/)

## 功能

- **SD 卡初始化**：使用 SDMMC 主机驱动程序挂载 SD 卡。
- **文件 I/O**：演示在 SD 卡上写入和读取文件。
- **卡信息**：检索并显示 SD 卡的名称、类型、容量和其他详细信息。
- **错误处理**：包括对 SD 卡操作期间常见错误的检查。

## 硬件要求

- **MCU**：ESP32-S3 开发板
- **SD 卡**：一张格式化为 FAT32 的 microSD 卡。

## 引脚连接

| 功能      | ESP32-S3 引脚 | 描述                                      |
| :------------ | :------------ | :---------------------------------------- |
| **SD 卡**     |               |                                           |
| SD CMD        | GPIO_NUM_10   | SD 卡命令线                               |
| SD D0         | GPIO_NUM_9    | SD 卡数据线 0                             |
| SD CLK        | GPIO_NUM_11   | SD 卡时钟线                               |

## 如何使用

1.  **准备 SD 卡**：将一张 microSD 卡格式化为 FAT32 文件系统。
2.  **连接硬件**：根据引脚连接表将 SD 卡连接到 ESP32-S3 开发板。
3.  **编译和烧录**：编译项目并将其烧录到 ESP32-S3 开发板。
4.  **监视输出**：打开串行监视器以查看 SD 卡测试的输出。

## 编译和烧录

### 环境设置

请确保您已安装并配置了 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 开发环境。

### 编译步骤

1.  **设置目标芯片**：
    ```bash
    idf.py set-target esp32s3
    ```

2.  **构建项目**：
    ```bash
    idf.py build
    ```

3.  **烧录和监视**：
    ```bash
    idf.py flash monitor
    ```