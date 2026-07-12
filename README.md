<div align="center">

# TOS Windows Tools

**PC 端工具集 — HID 诊断 + SBL FASTBOOT 烧录 + TOS Debug Bridge**

[![Platform](https://img.shields.io/badge/Platform-Windows-blue.svg)]()
[![Language](https://img.shields.io/badge/Language-Python_3.11-yellow.svg)]()
[![License](https://img.shields.io/badge/License-GPLv2-green.svg)]()

</div>

---

## 概述

TOS Windows Tools 提供三个独立工具，用于与 TOS 嵌入式设备通信：

| 工具 | 用途 | 协议 |
|------|------|------|
| **TOS Helper** | HID 自定义报告收发诊断 (GUI + CLI) | HID Vendor (PID 0x5750) |
| **sbltool** | SBL FASTBOOT 固件烧录、分区管理、OEM 解锁 | CDC (PID 0x5751) |
| **tdb** | TOS Debug Bridge：设备 Shell、文件推送/拉取 | CDC (PID 0x5753) |

详细技术说明见 [docs/technical-docs.md](docs/technical-docs.md)。

---

## 快速开始

### 前置条件

- Windows 10 / 11
- **Miniconda** 或 **Anaconda**（Python 3.11）

### 一键安装

```bat
cd /d C:\Code\tos\win-pc
scripts\setup-conda.bat
```

自动完成：创建 `tos` Conda 环境 → 安装依赖 → 构建 `TOS Helper.exe` / `sbltool.exe` / `tdb.exe`。

### 分步构建

```bat
conda activate tos
python -m pip install -r requirement.txt
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
```

默认三合一构建：TOS Helper + sbltool + tdb 一次完成。

### 安装 CLI 到 PATH

```bat
conda activate tos
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Install
```

安装后，在 `tos` 环境下可直接使用 `sbltool` 和 `tdb`。

---

## 设备发现与通信

所有工具通过枚举 COM 口并匹配 USB VID/PID 来发现设备。

```bat
# 列出 SBL 设备
sbltool devices

# 使用 STM32 UID 指定设备（推荐，COM 口不可靠）
sbltool -s <serial> getvar all

# TDB 交互式 Shell
tdb -s <serial> shell

# 文件传输
tdb -s <serial> push local.bin 0:/data/local.bin
tdb -s <serial> pull 0:/data/settings.bin

# 烧录
sbltool -s <serial> flash system system.bin
sbltool -s <serial> erase tmp
sbltool -s <serial> reboot recovery
sbltool -s <serial> oem unlock
```

**重要**: STM32 UID 是稳定标识；COM 端口号仅用于调试，不可依赖。

---

## 项目结构

```text
win-pc/
├── src/
│   ├── tos_helper/          # TOS Helper GUI + CLI
│   │   ├── app.py           # 主逻辑（~557 行）+ 入口
│   │   ├── tos_helper.spec  # PyInstaller 配置
│   │   └── assets/          # 图标
│   ├── sbltool/             # SBL 烧录 CLI
│   │   ├── app.py           # ~458 行 + 入口
│   │   └── sbltool.spec
│   └── tdb/                 # TOS Debug Bridge CLI
│       ├── app.py           # ~600 行 + 入口
│       └── tdb.spec
├── scripts/                 # 构建/安装脚本
│   ├── build.ps1            # 三合一构建引擎（支持 -Install）
│   ├── clean.bat            # 清理 build/ + dist/ + Python 缓存
│   └── setup-conda.bat      # 一键环境配置 + 构建
├── docs/                    # 文档
├── environment.yml          # Conda 环境定义
├── requirement.txt          # pip 依赖
├── LICENSE                  # GPLv2
└── README.md
```

---

## 文档

| 文档 | 说明 |
|------|------|
| [technical-docs.md](docs/technical-docs.md) | 架构、通信协议、设备发现、构建发布 |
| [style-docs.md](docs/style-docs.md) | Python 代码风格规范 |
| [update-docs.md](docs/update-docs.md) | 版本更新记录 |

---

## 构建产物

```text
dist/
├── TOS Helper.exe           # 独立 GUI 工具
└── platform-tools/
    ├── sbltool.exe          # 烧录 CLI
    └── tdb.exe              # 调试桥 CLI
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
