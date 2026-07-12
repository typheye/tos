<div align="center">

# TOS ESP8266 AT 固件

**ESP8266 WiFi 模块 AT 固件 — 集成于 TOS 系统的串口转 WiFi 桥接器**

[![Platform](https://img.shields.io/badge/Platform-ESP8266-blue.svg)]()
[![AT Version](https://img.shields.io/badge/AT-v1.8.x-green.svg)]()
[![License](https://img.shields.io/badge/License-GPLv2-green.svg)]()

</div>

---

## 概述

本仓库提供 TOS 系统中 ESP8266 WiFi 模块所需的 **AT 固件**（预编译二进制）。ESP8266 通过 UART 与主控 STM32F407（slave-board）通信，使用标准 AT 指令集实现 WiFi 联网、TCP Socket 通信等功能。烧录工具和串口调试终端由乐鑫/安信可官方提供。

详细技术说明见 [docs/technical-docs.md](docs/technical-docs.md)。

---

## 目录结构

```text
esp8266-code/
├── firmware/                     # ESP8266 AT 固件
│   └── ESP8266-AT-1M.bin        # 标准 AT 固件 (1 MB，v1.8.x)
├── docs/                         # 文档
├── LICENSE                       # GPLv2
└── README.md
```

---

## 固件烧录

### 准备工作

1. 将 ESP8266 的 GPIO0 拉低进入下载模式
2. 连接 USB-TTL 转换器（TX→RX, RX→TX, GND→GND）
3. 下载 [Espressif Flash Download Tool](https://www.espressif.com/en/support/download/other-tools)（乐鑫官方烧录工具）

### 烧录步骤

1. 打开 Flash Download Tool，选择 **ESP8266** > **SPI Download**
2. 配置参数：

   | 参数 | 值 |
   |------|-----|
   | SPI Speed | 40 MHz |
   | SPI Mode | DOUT |
   | Flash Size | 16Mbit (2MB) |
   | 波特率 | 115200 |
   | 地址 | 0x0 |
   | 固件路径 | `firmware/ESP8266-AT-1M.bin` |

3. 点击 START 开始烧录

### 验证

烧录后使用任意串口终端工具（如 PuTTY、[AiThinker Serial Tool](https://docs.ai-thinker.com/)）检查 AT 应答：

```text
打开 COM 口 115200-8N1
发送: AT
期望: OK
发送: AT+GMR
期望: AT version:1.8.x...
```

---

## 集成于 TOS 主控

ESP8266 在 STM32 侧由 `slave-board/src/hardware/esp8266.cpp` / `esp8266.hpp` 驱动：

| 功能 | C 接口 | C++ 方法 |
|------|--------|----------|
| 初始化 | `ESP8266_Init()` | `ESP8266::init()` |
| 连接 WiFi | `ESP8266_ConnectWiFi(ssid, pwd)` | `esp8266.connectWiFi(ssid, pwd)` |
| TCP 连接 | `ESP8266_StartTCP(host, port)` | `esp8266.startTCP(host, port)` |
| 发送数据 | `ESP8266_SendData(data, len)` | `esp8266.sendData(data, len)` |
| 是否已连接 | `ESP8266_IsConnected()` | `esp8266.isConnected()` |
| 读取 IP | `ESP8266_GetIP(buf, sz)` | `esp8266.getIP(buf, sz)` |
| 读取 RSSI | `ESP8266_GetRSSI(rssi)` | `esp8266.getRSSI(rssi)` |
| 恢复 | `ESP8266_TryRecover(force)` | `esp8266.tryRecover(force)` |

驱动特点：

- **硬件控制**：EN (GPIOF0) 和 RST (GPIOF1) 引脚控制模块上下电和复位
- **自动恢复**：UART RX 失效检测 → 重新使能中断 → 硬件复位 → 重试 AT 初始化
- **LED 指示**：通信成功/失败时通过板载 LED 指示

---

## 文档

| 文档 | 说明 |
|------|------|
| [technical-docs.md](docs/technical-docs.md) | ESP8266 集成架构、AT 协议、故障恢复 |
| [update-docs.md](docs/update-docs.md) | 版本更新记录 |

---

## 许可证

Copyright (c) 2021-2026 Typheye

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

> **声  明**：ESP8266 AT 固件二进制文件来自 Espressif 官方发布。Flash Download Tool 和串口终端工具为各自版权所有者财产，请从官方渠道获取。
