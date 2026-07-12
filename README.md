<div align="center">

# TOS · Slave Board Firmware

**STM32F407 从板固件 — ECDSA 安全启动 + FASTMAP 分区**

[![Platform](https://img.shields.io/badge/Platform-STM32F407-orange.svg)]()
[![Language](https://img.shields.io/badge/Language-C/C++-blue.svg)]()
[![Status](https://img.shields.io/badge/Status-Active-brightgreen.svg)]()

</div>

## 概述

Slave Board 运行于 STM32F407ZGT6 (1 MB Flash)，采用 **FASTMAP 5 分区**，**ECDSA P-256 + SHA-256 安全启动**，**SD 卡持久化配置**。

## 快速开始

```bash
# 配置
cmake --preset Release

# 构建 + 签名
cmake --build build/Release --target factory_images -- -j24

# 全量烧录
scripts/upload_factory.bat

# 仅烧录 SYSTEM
scripts/upload.bat system
```

**VSCode**: `Ctrl+Shift+B` → 一键构建签名烧录。

**解锁开发模式**: `sbltool oem unlock` → 按按键确认 → 可刷未签名镜像。

## 文档

| 文档 | 说明 |
|------|------|
| [technical-docs.md](docs/technical-docs.md) | 技术路线、启动流程、移植指南 |
| [partition-docs.md](docs/partition-docs.md) | FASTMAP 分区布局详解 |
| [secure-docs.md](docs/secure-docs.md) | 安全策略、签名机制、OEM Lock |
| [style-docs.md](docs/style-docs.md) | 代码风格规范 |
| [syslog-docs.md](docs/syslog-docs.md) | 日志系统说明 |
| [update-docs.md](docs/update-docs.md) | 更新记录、修改日志 |

## 项目结构

```
slave-board/
├── partitions/       # 分区固件 (ELF, SBL, REC)
├── src/              # SYSTEM 主应用
├── ld/               # 链接脚本
├── cmake/            # CMake 构建
├── scripts/          # 签名、烧录、导出
└── docs/             # 文档
```

## 许可证

Copyright (c) 2021-2026 Typheye. All rights reserved.
