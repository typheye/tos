# 技术手册

> 版本: v1.0 | 语言: Python 3.11 | 平台: Linux / Windows

---

## 1. 项目概述

**TOS PC Tools** 是一组用于与 TOS 嵌入式固件交互的 PC 端工具，包含 TOS Helper、tvidtool、sbltool、tdb 四个工具：

| 组件 | 类型 | 协议 | USB ID | 用途 |
|------|------|------|--------|------|
| **TOS Helper** | GUI (Tkinter) + CLI | HID Vendor Report | VID 0x0483, PID 0x5750 | HID 设备诊断、自定义报告收发 |
| **sbltool** | CLI | SBL FASTBOOT CDC | VID 0x0483, PID 0x5751 | 固件烧录、分区管理、OEM 解锁 |
| **tdb** | CLI (交互式 Shell) | TOS Debug Bridge (REC) | VID 0x0483, PID 0x5756 | 设备 Shell、文件推送/拉取 |

---

## 2. 通信协议

### 2.1 设备发现

TOS Helper 通过 HID API 枚举设备，SBL/TDB 通过 USB CDC 串口和协议探测发现设备。使用 STM32 UID 作为稳定标识，COM 端口号仅为传输层细节，不视为稳定身份。

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

**flash 超时说明**：

| 阶段 | 超时 | 说明 |
|------|------|------|
| FLASHBEGIN | 120s | 容纳大分区擦除（system 768 KB 约需 12s） |
| FLASHDATA 握手 | 10s | 设备在擦除完成后响应 `OKAY SEND` |
| FLASHDATA 数据块 | 20s | 16 KB 写入约 160ms |
| FLASHEND | 35s | CRC 重读校验 + 安全签名验证 |
| ERASE (system/tmp) | 90s | 大分区擦除 |
| ERASE (其他) | 25s | 小分区擦除 |



### 2.4 tdb — TOS Debug Bridge 协议

| 属性 | 值 |
|------|----|
| USB VID | 0x0483 |
| USB PID | 0x5756 |
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

## 3. 统一项目与构建

正式项目为 `pc-app/`。`src/` 下四个工具共享 `tos_common` 的元数据、资源路径和系统适配。
各工具版本只维护 `metadata.json`；PyInstaller 统一配置为 `packaging/tos.spec`。

Linux 使用 `bash scripts/pc.sh <操作>`，Windows 使用 `./scripts/pc.cmd <操作>`。
操作包括 setup/run/build/install/clean/test/doctor。默认使用 tos Conda 环境，
`--current-env` 支持 venv 与 CI；所有参数用列表传递，失败码逐层返回。
详见 [开发指南](developer-guide.md) 与 [README](../README.md)。

## 4. 构建与安装

只构建当前宿主平台，Windows 生成 .exe，Linux 生成原生 ELF；采用 PyInstaller 单文件打包，
运行时会使用临时解包目录。版本检查通过后才发布 dist，构建失败保留旧版。
Windows 文件属性从元数据动态生成，Helper 保留图标和 DPI manifest。

安装位于用户独立目录，Windows 快捷方式使用安装内图标；Linux 不创建启动项。
clean 默认只清理缓存，
clean --all 才额外删除 dist，均不删除安装或 ~/.tdb_device.json。

## 5. 平台约束

Python 3.11+，Linux / Windows 10/11，Tkinter GUI，hidapi 0.15.x。
Windows 的 COM 访问权限提示和 Linux 的 dialout 提示分支隔离。
Windows Shell 补全依赖 pyreadline3；Linux 使用标准 readline。
原有 USB 协议、传输超时、CRC32、STM32 UID 发现和 TVID RGB565 格式保持不变。

## 6. 调试

```bat
# 查看 HID 设备列表
"TOS Helper.exe" --list

# 串口调试（手动测试）
sbltool devices
tdb devices

# 查看 SBL 变量
sbltool -s <serial> getvar all

# 交互式设备 Shell
tdb -s <serial> shell
```
