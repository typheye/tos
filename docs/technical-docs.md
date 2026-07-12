# 技术手册

> 版本: v1.0 | 语言: Python 3.11 | 平台: Windows

---

## 1. 项目概述

**TOS Windows Tools** 是一组用于与 TOS 嵌入式固件交互的 PC 端工具，包含三个独立组件：

| 组件 | 类型 | 协议 | USB ID | 用途 |
|------|------|------|--------|------|
| **TOS Helper** | GUI (Tkinter) + CLI | HID Vendor Report | VID 0x0483, PID 0x5750 | HID 设备诊断、自定义报告收发 |
| **sbltool** | CLI | SBL FASTBOOT CDC | VID 0x0483, PID 0x5751 | 固件烧录、分区管理、OEM 解锁 |
| **tdb** | CLI (交互式 Shell) | TOS Debug Bridge (REC) | VID 0x0483, PID 0x5753 | 设备 Shell、文件推送/拉取 |

---

## 2. 通信协议

### 2.1 设备发现

所有工具通过枚举可用 COM 口、匹配 USB VID/PID 来发现设备。使用 STM32 UID 作为稳定标识，COM 端口号仅为传输层细节，不视为稳定身份。

### 2.2 TOS Helper — HID 协议

| 属性 | 值 |
|------|----|
| USB VID | 0x0483 (STMicroelectronics) |
| USB PID | 0x5750 |
| 接口 | HID Vendor Collection |
| Report ID | 0x10 |
| 报告长度 | 64 字节 |
| 通信模式 | OUT 报告（PC→设备）、IN 报告（设备→PC） |
| Python 依赖 | `hidapi` |

支持命令行模式进行一次性收发，也提供 Tkinter GUI 用于持续诊断。

### 2.3 sbltool — FASTBOOT CDC 协议

| 属性 | 值 |
|------|----|
| USB VID | 0x0483 |
| USB PID | 0x5751 |
| 通信方式 | USB CDC (虚拟串口) |
| 波特率 | 忽略 (USB 虚拟) |
| Python 依赖 | `pyserial` |

**支持的命令**:

| 命令 | 说明 |
|------|------|
| `devices` | 列出所有连接的 SBL 设备 |
| `getvar <var>` | 读取设备变量（如 product, serialno, version） |
| `getvar all` | 读取全部变量 |
| `flash <partition> <image>` | 烧录分区镜像 |
| `erase <partition>` | 擦除分区 |
| `reboot [bootloader\|recovery\|system]` | 重启设备 |
| `oem unlock\|lock` | OEM 解锁/锁定 |
| `partitions` | 列出分区表 |

> sbltool 不嵌入分区表。分区信息通过 `GETVAR` 和 `OEM PARTITIONS` 从已连接的 SBL 获取。

### 2.4 tdb — TOS Debug Bridge 协议

| 属性 | 值 |
|------|----|
| USB VID | 0x0483 |
| USB PID | 0x5753 |
| 通信方式 | USB CDC (虚拟串口) |
| 协议 | ASCII 文本 + CRC32 |
| Python 依赖 | `pyserial`, `pyreadline3` |

**支持的功能**:

| 功能 | 说明 |
|------|------|
| `devices` | 列出 REC 设备 |
| `shell` | 交互式设备 Shell（或单条命令） |
| `push <local> <remote>` | 上传文件到设备 SD 文件系统 |
| `pull <remote> [local]` | 从设备下载文件 |
| `info` | 设备状态信息 |

`push`/`pull` 使用 CRC32 校验保证传输完整性。

---

## 3. 项目结构

```text
win-pc/
├── src/
│   ├── tos_helper/          # TOS Helper GUI + CLI
│   │   ├── app.py           # ~557 行 — 主逻辑 + 入口
│   │   ├── metadata.json
│   │   ├── tos_helper.spec   # PyInstaller 配置
│   │   ├── tos_helper.manifest  # DPI 感知清单
│   │   ├── version_info.txt  # Windows 版本资源
│   │   └── assets/           # 图标资源
│   ├── sbltool/             # SBL FASTBOOT CLI
│   │   ├── app.py           # ~458 行 + 入口
│   │   ├── metadata.json
│   │   ├── sbltool.spec
│   │   └── version_info.txt
│   └── tdb/                 # TOS Debug Bridge CLI
│       ├── app.py           # ~600 行 + 入口
│       ├── metadata.json
│       ├── tdb.spec
│       └── version_info.txt
├── scripts/                 # 构建脚本
│   ├── build.ps1            # 构建引擎（三合一，支持 -Install 安装到 PATH）
│   ├── clean.bat            # 清理 build/ + dist/ + Python 缓存
│   └── setup-conda.bat      # 一键 Conda 环境配置 + 构建
├── docs/                    # 文档
├── environment.yml          # Conda 环境定义
├── requirement.txt          # pip 依赖
├── LICENSE                  # GPLv2
└── README.md
```

---

## 4. 构建与发布

### 4.1 前置条件

- **Miniconda** 或 **Anaconda** (Python 3.11)
- Windows 10 / 11

### 4.2 一键构建

```bat
cd /d C:\Code\tos\win-pc
scripts\setup-conda.bat
```

此脚本自动：创建/更新 `tos` Conda 环境 → 安装依赖 → 构建三个可执行文件。

### 4.3 分步构建

```bat
conda activate tos
python -m pip install -r requirement.txt
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1
```

### 4.4 安装 CLI 到 PATH

`build.ps1` 支持 `-Install` 开关，将 `sbltool.exe` 和 `tdb.exe` 复制到 conda 环境的 `Scripts/` 目录：

```bat
conda activate tos
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build.ps1 -Install
```

安装后可直接在 `tos` 环境下使用 `sbltool` 和 `tdb`。

### 4.5 清理

```bat
scripts\clean.bat
```

默认清理所有产物：`build/`、`dist/`、`__pycache__/`、`*.pyc`、`.pytest_cache/` 等。

### 4.6 发布流程

1. 更新 `src/*/metadata.json` 中的版本号
2. 更新 `src/*/version_info.txt` 中的 FileVersion
3. 更新 `docs/update-docs.md` 添加变更记录
4. 执行 `scripts\setup-conda.bat` 全量构建
5. 验证 `dist/` 下各可执行文件功能正常
6. 提交 TAG

---

## 5. 约束

| 项目 | 值 |
|------|-----|
| Python 最低版本 | 3.11 |
| 支持操作系统 | Windows 10 / 11 |
| CLI 安装方式 | `build.ps1 -Install`（添加到 conda Scripts） |
| GUI 分辨率 | PerMonitorV2 DPI 感知 |
| HID 库 | `hidapi`（C 扩展） |
| 串口库 | `pyserial` |
| 构建工具 | PyInstaller ≥ 6.3 |
| 打包方式 | 单文件 `.exe`（无临时目录） |

---

## 6. 调试

```bat
# 查看 HID 设备列表
"TOS Helper.exe" --list

# 串口调试（手动测试）
sbltool devices -v
tdb devices -v

# 查看 SBL 变量
sbltool -s <serial> getvar all

# 交互式设备 Shell
tdb -s <serial> shell
```
