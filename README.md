<div align="center">

# TOS PC Tools (Linux)

**Linux 上位机工具 — TOS Helper / tvidtool / sbltool / tdb**

[![Platform](https://img.shields.io/badge/Platform-Linux-blue.svg)]()
[![Language](https://img.shields.io/badge/Language-Python-yellow.svg)]()
[![Packaging](https://img.shields.io/badge/Packaging-PyInstaller-lightgrey.svg)]()

</div>

---

## 概述

STM32F407 固件的 Linux 上位机工具，使用 PyInstaller 打包为独立可执行文件
（`dist/TOS Helper` 和 `dist/platform-tools/{tvidtool,sbltool,tdb}`）。与
[win-pc](../win-pc/README.md) 使用同一套工具源码，仅构建配置和系统适配不同。

| 工具 | 版本 | 说明 |
|------|------|------|
| `TOS Helper` | v1.0.1 | HID 自定义报告收发诊断 (GUI + CLI，PID 0x5750) |
| `tvidtool` | v0.1.0 | 将普通视频转换为 `.tvid` 流，供 SD 卡播放 |
| `sbltool` | v1.0.2 | SBL FASTBOOT USB CDC 烧录协议主机 (PID 0x5751) |
| `tdb` | v1.0.1 | TOS Debug Bridge REC USB CDC 主机，文件/设置/诊断 (PID 0x5756) |

---

## 环境

构建使用 `tos` Conda 环境。首次创建：

```bash
conda env create -f environment.yml
# 或，在已有环境下：
python -m pip install -r requirement.txt
```

依赖：`pyinstaller`、`opencv-python`、`pyserial`、`hidapi`、`tkinter`。

> USB 权限：访问 USB CDC 设备需将用户加入 `dialout` 组并重新登录：
> `sudo usermod -aG dialout $USER`。工具在权限不足时会给出对应提示。

---

## TVID Tool

将普通视频转换为 `.tvid` 流，固件可从 SD 卡播放。打包后使用内置 OpenCV，无需在 `PATH` 中额外提供 `ffmpeg`。

```bash
python3 src/tvidtool/app.py input.mp4 -o test.tvid --fps 60 --size 240x240
```

仅导出第 0 到 999 帧（结束帧不包含）：

```bash
python3 src/tvidtool/app.py input.mp4 -o test.tvid --fps 60 --size 240x240 --start-frame 0 --end-frame 1000
```

将输出拷贝到 SD 卡：

```text
0:/storage/test.tvid
```

文件存在且有效时，进入板端 `Demo → TVID Test` 播放，按 ENTER 退出。

> 注意：请用当前工具重新生成旧的 `.tvid` 文件。固件快速路径要求 LCD 就绪的大端 RGB565 载荷。

---

## SBL Tool

`sbltool` 通过 USB CDC 与设备 FASTBOOT 协议通信。运行方式：

```bash
python3 src/sbltool/app.py --help
python3 src/sbltool/app.py examples
```

常用命令：

```bash
# 设备探测
sbltool devices

# 读取变量 / 分区表
sbltool getvar all
sbltool partitions

# 烧录（sbl/rec 为分阶段，system 为直写）
sbltool --wait flash sbl /path/to/sbl.bin
sbltool --wait flash rec /path/to/rec.bin
sbltool flash system /path/to/system.bin

# 擦除
sbltool erase rec
sbltool erase system

# 重启
sbltool reboot
sbltool reboot recovery
sbltool reboot bootloader

# OEM 解锁 / 上锁（解锁会清除全部用户数据）
sbltool oem unlock
sbltool oem lock
```

---

## TDB

`tdb` 是 REC 模式下的调试桥（TOS Debug Bridge），提供设备 Shell 与文件传输。

```bash
python3 src/tdb/app.py --help
```

常用命令：

```bash
# 设备探测 / 连接 / 断开
tdb devices
tdb connect
tdb disconnect

# 交互式 Shell，或执行单条命令
tdb shell
tdb shell ls

# 文件推送 / 拉取
tdb push local.bin 0:/storage/local.bin
tdb pull 0:/storage/file.bin ./file.bin
```

连接参数：`-s/--serial`、`-b/--baud`、`--timeout`。

---

## 构建

```bash
./scripts/build.sh              # 构建 TOS Helper 和三个命令行工具
./scripts/build.sh --install    # 同时复制到当前 Conda 环境的 bin/
```

脚本会在检测到 conda 时自动切换到 `tos` 环境后重新执行。

## 清理

```bash
./scripts/clean.sh
```

在 VS Code 中，**Build** 与 **Clean** 任务封装了相同的脚本。

---

## 项目结构

```text
linux-pc/
├── src/                       # 源码
│   ├── tos_helper/            # HID 诊断 (GUI + CLI)
│   ├── tvidtool/              # 视频 → .tvid 转换
│   ├── sbltool/               # FASTBOOT 烧录主机
│   └── tdb/                   # 调试桥
├── scripts/                   # 构建 / 清理脚本
│   ├── build.sh
│   └── clean.sh
├── environment.yml            # Conda 环境定义
├── requirement.txt            # pip 依赖
├── build/                     # 构建输出 (gitignored)
└── dist/                      # TOS Helper + platform-tools (gitignored)
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
