<div align="center">

# TOS · Slave Board Firmware

**STM32F407ZGT6 从板固件 — ECDSA P-256 安全启动 + FASTMAP 5 分区**

[![Platform](https://img.shields.io/badge/Platform-STM32F407-orange.svg)]()
[![Language](https://img.shields.io/badge/Language-C/C++-blue.svg)]()
[![Status](https://img.shields.io/badge/Status-Active-brightgreen.svg)]()

</div>

---

## 概述

Slave Board 采用 **FASTMAP 5 分区**（ELF / SBL / REC / TMP / SYSTEM），**ECDSA P-256 + SHA-256 安全启动**，**SD 卡持久化**。运行于 STM32F407ZGT6 (1 MB Flash)。

| 版本 | 日期 | 说明 |
|------|------|------|
| v3.2 | 2026-07-12 | 串口修复、OEM UNLOCK 简化、Headless 模式 |
| v3.1 | 2026-07-12 | SD 持久化、文档重组 |
| v3.0 | 2026-07-10 | FASTMAP 落地、全链安全启动、代码风格标准化 |

详细更新记录见 [docs/update-docs.md](docs/update-docs.md)。

---

## 快速开始

```bash
# 1. 配置（首次会自动生成 ECDSA 签名密钥）
cmake --preset Release

# 2. 构建 + 签名 + 导出 BIN
cmake --build build/Release --target factory_images

# 3. 全量烧录（ELF + SBL + REC + SYSTEM）
scripts/flash.bat factory

# 仅烧录 SYSTEM
scripts/flash.bat system

# 烧录 REC + SYSTEM
scripts/flash.bat runtime
```

> **VSCode**: `Ctrl+Shift+B` → 选择 `factory_images` / `flash_factory` / `flash_system`。

**解锁开发模式**:
```bash
sbltool oem unlock
# LCD 显示警告 → 按键确认 → 重启后 unlocked
```

---

## 分区布局

| # | 扇区 | 地址 | 大小 | 分区 | 功能 | 安全 |
|---|------|------|------|------|------|------|
| 0 | 0 | `0x08000000` | 16 KB | **ELF** | 信任根：验证 SBL、状态记录 | 不可变 (1→0 直写) |
| 1 | 1–3 | `0x08004000` | 48 KB | **SBL** | 二级引导、FASTBOOT、LCD splash | ECDSA |
| 2 | 4 | `0x08010000` | 64 KB | **REC** | 恢复模式：SD 升级/格式化/TDB | ECDSA |
| 3 | 5 | `0x08020000` | 128 KB | **TMP** | 升级暂存区 (易失) | — |
| 4 | 6–11 | `0x08040000` | 768 KB | **SYSTEM** | 主应用 + 文件系统 | ECDSA |

详细分区说明见 [docs/partition-docs.md](docs/partition-docs.md)。

---

## 启动链

```text
上电
  │
  ▼
ELF · 信任根 (扇区 0 不可变)
  │  ├─ Cust_Setup(GPIO)
  │  ├─ ECDSA P-256 验证 SBL (~2s)
  │  └─ 写入状态记录 → Jump(SBL)
  │
  ▼
SBL · 二级引导 + FASTBOOT
  │  ├─ LCD splash
  │  ├─ ECDSA P-256 验证 SYSTEM/REC
  │  ├─ OEM Unlock 时跳过验签
  │  └─ 热重启 0.5s (RESTART 标志)
  │
  ▼
SYSTEM · 主固件 (或 REC 恢复模式)
```

| 场景 | 耗时 |
|------|------|
| 冷启动 (上电) | ~2s |
| 软复位 (RESTART) | ~0.5s |
| 定向重启 (boot_target) | ~0.5s |

详细技术路线见 [docs/technical-docs.md](docs/technical-docs.md)。

---

## 文档

| 文档 | 说明 |
|------|------|
| [technical-docs.md](docs/technical-docs.md) | 技术路线、启动流程、约束、移植指南 |
| [partition-docs.md](docs/partition-docs.md) | FASTMAP 分区布局、写权限、配置宏 |
| [secure-docs.md](docs/secure-docs.md) | 安全策略、ECDSA 签名、OEM Lock/Unlock |
| [style-docs.md](docs/style-docs.md) | 代码风格规范 (C/C++ 命名、缩进、include 守卫) |
| [syslog-docs.md](docs/syslog-docs.md) | Syslog 日志系统：等级、格式、SD 卡存储 |
| [update-docs.md](docs/update-docs.md) | 版本更新记录 |

---

## 项目结构

```text
slave-board/
├── Core/                 # CubeMX 生成 (main.c, main.h, startup, HAL)
├── Drivers/              # STM32 HAL + CMSIS
├── FATFS/                # FatFs 中间件 (CubeMX)
├── USB_DEVICE/           # USB CDC (CubeMX)
├── Middlewares/           # 第三方中间件
├── partitions/           # 分区固件
│   ├── manifest.h        # 分区配置宏 (单来源)
│   ├── ELF/              # 信任根 ~5 KB
│   ├── SBL/              # 二级引导 + FASTBOOT
│   └── REC/              # 恢复模式 (TDB 设置、SD 操作)
├── src/                  # SYSTEM 主应用
│   ├── core/             # 核心：初始化、管理器、SDK、系统服务
│   ├── library/          # 库：文件系统、JSON、3D 渲染、表情
│   ├── hardware/         # 驱动：LCD、ESP8266、Key、SDIO 等
│   └── gui/              # 界面：SysUI、Launcher、Activity、组件
├── ld/                   # 链接脚本 (ELF / SBL / REC / SYSTEM)
├── cmake/                # CMake 构建配置 + partitions 子项目
├── scripts/              # 工具脚本
│   ├── flash.bat         # OpenOCD 烧录 (factory/system/runtime)
│   ├── export.ps1        # 构建后导出 BIN + 签名 + 验证
│   ├── sign.ps1          # ECDSA P-256 签名
│   ├── verify.ps1        # 签名验证
│   └── keygen.ps1        # 密钥生成 + C 头文件导出
├── docs/                 # 文档
├── build/                # 构建输出 (gitignored)
└── dist/                 # 产物 (BIN/ELF/CSV, gitignored)
```

---

## 安全机制

- **全链签名**: ELF → SBL → SYSTEM/REC 每级 ECDSA P-256 强制验证
- **ELF 不可变**: 扇区 0 出厂后只接受 1→0 直写、无擦除操作
- **OEM Lock**: 锁定状态下禁止刷写未签名镜像；验证失败自动回滚
- **环形状态日志**: ELF 尾部 4 KB 区域，64 条 × 64B，记录 boot_target / unlock / upgrade_txn

详细安全策略见 [docs/secure-docs.md](docs/secure-docs.md)。

---

## 移植

### 同系列 (STM32F4xx)

1. 修改 `partitions/manifest.h` 中的分区地址/大小
2. 同步更新 `ld/*.ld` 的 `FLASH ORIGIN` / `LENGTH`
3. 按需调整 `Cust_Setup()` (GPIO 初始化)
4. `LCD_ENABLED=0` 关闭 LCD (Headless 模式)

### 签名密钥

```bash
# 删除旧密钥 → cmake 重新配置时自动生成新密钥
rm sign/development-signing-key.pk8*
cmake --preset Release
cmake --build build/Release --target factory_images
```

---

## 调试

```bash
# 串口日志: USART1 PA9/PA10 115200-8N1
# 格式: [timestamp] [LEVEL] [MOD  ] message

# OpenOCD
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
  -c "init" -c "halt" -c "reg pc" -c "shutdown"

# FASTBOOT (USB CDC)
sbltool getvar all
sbltool devices
sbltool oem unlock
```

---

## 日志系统

| 等级 | 值 | 串口 | 文件 | 典型 |
|------|----|------|------|------|
| FATAL | 0 | ✅ | ✅ | 系统崩溃 |
| ERROR | 1 | ✅ | ✅ | 外设失败 |
| WARN | 2 | ✅ | ✅ | 降级行为 |
| INFO | 3 | ✅ | ✅ | 状态信息 |
| DEBUG | 4 | ✅ | 编译可选 | 调试 |

详细日志系统说明见 [docs/syslog-docs.md](docs/syslog-docs.md)。

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

Third-party components (STM32 HAL, CMSIS, FatFs, USB Device Library)
retain their original licenses as documented in their respective
directories.
