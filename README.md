<div align="center">

# TOS — Typheye Operating System

**嵌入式物联网平台 · STM32F407 安全固件 + PC 工具 + WiFi 联网**

[![Platform](https://img.shields.io/badge/Platform-STM32F407-orange.svg)]()
[![Language](https://img.shields.io/badge/Language-C%2FC%2B%2B%20%7C%20Python-blue.svg)]()
[![Boot](https://img.shields.io/badge/Boot-ECDSA%20P%2B256%20Secure-brightgreen.svg)]()
[![License](https://img.shields.io/badge/License-GPLv2-green.svg)]()

</div>

---

## 概述

TOS 是一个运行于 **STM32F407ZGT6** 上的嵌入式系统，采用 **ECDSA P-256 + SHA-256 安全启动**、**FASTMAP 5 分区**布局，提供 LCD 图形界面、WiFi 联网（ESP8266）、SD 卡持久化等功能。上位机 PC 工具涵盖 HID 诊断、固件烧录、调试桥接。

项目分为三个仓库：

| 仓库 | 说明 | 语言 |
|------|------|------|
| **[slave-board](slave-board/)** | STM32F407 从板固件：安全启动链、GUI、驱动、文件系统 | C / C++ |
| **[win-pc](win-pc/)** | Windows 上位机工具：HID 诊断、FASTBOOT 烧录、Debug Bridge | Python |
| **[esp8266-code](esp8266-code/)** | ESP8266 WiFi AT 固件与集成说明 | — |

---

## 系统架构

### 三层结构

| 层 | 组件 | 职责 | 通信 |
|----|------|------|------|
| **主控** | `slave-board` · STM32F407ZGT6 | 安全启动、GUI、驱动、文件系统 | UART2 → ESP8266 |
| **联网** | `esp8266-code` · ESP8266 | AT 指令 WiFi 桥接、TCP Socket | UART2 ← STM32 |
| **上位机** | `win-pc` · Windows 工具 | HID 诊断、FASTBOOT 烧录、调试桥 | USB CDC / HID |

### slave-board 固件分区

| 分区 | 大小 | 功能 | 安全 |
|------|------|------|------|
| **ELF** | 16 KB | 信任根：验证 SBL、不可变 | ECDSA 强制 |
| **SBL** | 48 KB | 二级引导 + FASTBOOT | ECDSA |
| **REC** | 64 KB | 恢复模式(SD 升级/TDB) | ECDSA |
| **TMP** | 128 KB | 升级暂存区(易失) | — |
| **SYSTEM** | 768 KB | 主固件：Core / driver / GUI / library | ECDSA |

### 安全启动链

```text
上电 → ELF (ECDSA 验证 SBL) → SBL (ECDSA 验证 SYSTEM/REC) → SYSTEM
                                └── OEM Unlock 时跳过验签
```

| 场景 | 耗时 |
|------|------|
| 冷启动（上电） | ~2s |
| 软复位（RESTART 标志） | ~0.5s |
| 定向重启（boot_target） | ~0.5s |

### win-pc 上位机工具

| 工具 | 类型 | 协议 | 用途 |
|------|------|------|------|
| **TOS Helper** | GUI + CLI | HID Vendor (PID 0x5750) | 自定义报告收发诊断 |
| **sbltool** | CLI | FASTBOOT CDC (PID 0x5751) | 固件烧录、分区、OEM 解锁 |
| **tdb** | CLI (Shell) | Debug Bridge (PID 0x5753) | 设备 Shell、文件推送/拉取 |

---

## 快速开始

### 全量构建固件

```bash
cd slave-board

# 配置（首次）
cmake --preset Release

# 构建 + 签名 + 导出 BIN 镜像
cmake --build build/Release --target factory_images

# 烧录（ELF + SBL + REC + SYSTEM）
scripts/flash.bat factory
```

### 构建 PC 工具

```bash
cd win-pc

# 一键构建（自动创建 conda 环境）
scripts\setup-conda.bat

# 或分步执行
conda activate tos
python -m pip install -r requirement.txt
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
```

### 烧录 ESP8266 AT 固件

详见 [esp8266-code/README.md](esp8266-code/README.md)。

---

## 文档

### 项目文档

| 文档 | 来源 | 说明 |
|------|------|------|
| [slave-board/README.md](slave-board/README.md) | slave-board | 固件概述、分区布局、启动链、调试 |
| [win-pc/README.md](win-pc/README.md) | win-pc | 上位机工具概述、构建、设备发现 |
| [esp8266-code/README.md](esp8266-code/README.md) | esp8266-code | ESP8266 固件与集成说明 |

### 子项目文档

| 文档 | 来源 | 说明 |
|------|------|------|
| [slave-board/docs/technical-docs.md](slave-board/docs/technical-docs.md) | slave-board | 技术路线、启动流程、约束、移植 |
| [slave-board/docs/partition-docs.md](slave-board/docs/partition-docs.md) | slave-board | FASTMAP 分区布局、写权限 |
| [slave-board/docs/secure-docs.md](slave-board/docs/secure-docs.md) | slave-board | 安全策略、ECDSA 签名、OEM Lock |
| [slave-board/docs/syslog-docs.md](slave-board/docs/syslog-docs.md) | slave-board | 日志系统：等级、格式、SD 卡存储 |
| [slave-board/docs/style-docs.md](slave-board/docs/style-docs.md) | slave-board | C/C++ 代码风格规范 |
| [slave-board/docs/update-docs.md](slave-board/docs/update-docs.md) | slave-board | 固件更新记录 |
| [win-pc/docs/technical-docs.md](win-pc/docs/technical-docs.md) | win-pc | PC 工具架构、通信协议、设备发现 |
| [win-pc/docs/style-docs.md](win-pc/docs/style-docs.md) | win-pc | Python 代码风格规范 |
| [win-pc/docs/update-docs.md](win-pc/docs/update-docs.md) | win-pc | PC 工具更新记录 |
| [esp8266-code/docs/technical-docs.md](esp8266-code/docs/technical-docs.md) | esp8266-code | ESP8266 硬件连接、AT 协议、故障恢复 |

---

## 项目结构

```text
tos/
├── main/                     # ← 顶层 README（本文件）
├── slave-board/              # STM32F407 固件 (C/C++, CMake)
│   ├── partitions/           # 分区固件：ELF / SBL / REC
│   ├── src/                  # SYSTEM 主应用
│   ├── scripts/              # 烧录、签名、导出脚本
│   └── docs/                 # 6 篇文档
├── win-pc/                   # Windows PC 工具 (Python, PyInstaller)
│   ├── src/                  # 源码：tos_helper / sbltool / tdb
│   ├── scripts/              # 构建、清理脚本
│   └── docs/                 # 3 篇文档
├── esp8266-code/             # ESP8266 AT 固件 + 集成指南
│   ├── firmware/             # AT 固件二进制
│   └── docs/                 # 2 篇文档
└── reasonix.toml             # Reasonix 配置
```

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
