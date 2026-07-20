# 更新记录

> 按时间倒序

---
## v1.0.2 (2026-07-20)

### Bug 修复

- **flash 命令超时**：修复非 staged 分区（system）在 FB 模式下 `flash` 命令超时的问题。
  根因：设备端 `SBL_FlashBegin()` 在响应 `FLASHBEGIN` 前同步擦除分区，
  system 分区 768 KB（6×128 KB 扇区）擦除约需 12 秒，超过 PC 端原有 5 秒超时。
  修复：将 `FLASHBEGIN` 响应超时从 5 秒增至 120 秒，
  `FLASHDATA` 头握手超时从 5 秒调整为 10 秒。

---

## v1.0.1 (2026-07-12)

### 重构构建系统

- `build.bat` 删除，仅保留 `build.ps1` 作为唯一构建引擎
- `build.ps1` 新增 `-Tool` 参数支持单工具构建（`tos_helper` / `sbltool` / `tdb`）
- `install-cli.bat` 删除，安装功能合并到 `build.ps1 -Install`
- `flash-system.bat` 删除（flash 功能由 sbltool 自身提供）
- `clean.bat` 重构：默认清理 `build/` + `dist/` + Python 缓存（`__pycache__/`、`*.pyc`、`.pytest_cache/` 等）
- `setup-conda.bat` 简化，不再传递 `-Install`

### 代码命名及版本同步

- 三个工具版本号统一升级至 **v1.0.1**（`app.py`、`metadata.json`、`version_info.txt`）
- 删除 `__init__.py` 和 `__main__.py`，入口和版本定义合并到各 `app.py`

### 文档与许可

- 添加 `docs/` 目录（technical-docs.md / style-docs.md / update-docs.md）
- 添加 GPLv2 `LICENSE` 文件
- Python 源文件头改为 Doxygen 风格（`@file`、`@author`、`@brief` + GPLv2 许可证块），匹配下位机样式
- style-docs.md 更新文件头模板
- README.md 重写：移除已删除脚本的引用，新增单工具构建说明

---

## v1.0.0 (2026-07-12)

### 初始发布

- **TOS Helper**: Tkinter GUI + CLI 工具，通过 HID Vendor Report 与 TOS 设备通信
  - 枚举 HID 集合（Keyboard、Mouse、Vendor）
  - Vendor 报告收发（Report ID 0x10）
  - CLI 模式支持 `--list`、`--send`、`--read`

- **sbltool**: SBL FASTBOOT CDC CLI
  - `devices` — 设备发现
  - `getvar` — 读取设备变量
  - `flash` — 分区烧录
  - `erase` — 分区擦除
  - `reboot` — 重启（bootloader/recovery/system）
  - `oem unlock/lock` — OEM 解锁/锁定
  - `partitions` — 分区表查询

- **tdb**: TOS Debug Bridge CLI（交互式 Shell）
  - `devices` — REC 设备发现
  - `shell` — 交互/单条命令执行
  - `push` — 文件上传（CRC32 校验）
  - `pull` — 文件下载（CRC32 校验）

### 构建系统

- Conda 环境管理（`environment.yml` → `tos` 环境）
- PyInstaller ≥ 6.3 单文件打包
- PowerShell 构建脚本（`build.ps1`），3 个工具依次构建
- 一键配置脚本（`setup-conda.bat`）
- CLI 安装脚本（`install-cli.bat`）

### 代码规范

- `from __future__ import annotations` 启用惰性类型注解
- 类型注解全覆盖公共接口
- USB VID/PID 集中管理
- 设备发现统一基于 VID/PID 匹配 + STM32 UID 稳定标识
