# 更新记录

> 按时间倒序

---

## v3.2 (2026-07-12)

### 串口修复
- **BRR 波特率计算修正** (`SBL_UartInit`): `f_CK/(16×BaudRate)` 替代错误的 `f_CK/BaudRate`，115200bps 正常输出

### OEM UNLOCK 简化
- 移除 token 验证，`sbltool oem unlock` 直接解锁
- 侧工具增加确认提示：显示警告文字 → 用户输入 yes/no → 确认后执行
- SBL 收到 `OEM UNLOCK` → 设 `boot_target=RECOVERY_FORMAT` → 重启格式化
- 已解锁设备再次执行时直接提示 "already unlocked"

### Headless 模式 (`LCD_ENABLED=0`)
- FASTBOOT 模式：不监听 PA15 按键，纯 USB CDC 循环
- **boardLED 状态指示**：FASTBOOT 常亮，SYSTEM 损坏快闪，REC 模式半秒闪烁
- 故障页（damage/recovery exception）5s 后自动重启，不再永久挂起

### 超时移除
- SBL: 删除 300s 空闲超时、30s 解锁超时、REC 损坏页 5s 倒计时
- REC: `MODE_WAIT` 永久运行（通过 TDB `reboot` 命令手动退出）；Format/Init/Upgrade 完成后自动重启

### REC 格式化增强
- `REC_MODE_FORMAT` 调用 `REC_FatInitStorage()`：除擦除 userdata 外，同时格式化 SD 卡（FAT32）并创建目录结构

### PC 工具修复
- `sbltool`: 所有命令 Ctrl+C 优雅退出；`oem unlock <hex>` 命令解析修复；新增 `reboot bootloader`
- `tdb`: `cd ../` 路径回退修复；`help` 输出每命令一行对齐

### 编译产物
- SBL: 24752 B (之前 26700 B)，使用率 50.36%
- REC: 51764 B，使用率 80.24%

## v3.1 (2026-07-12)

- SD 持久化路径修正 (`0:/data/settings.bin`)
- settings 结构增加 CRC-32 校验
- `SM_Save` 去 `g_sd_available` 预检, 每次直接写 SD
- `SM_Mount` 增加 `FR_NOT_READY` 重试 (3 次 × 200ms)
- PC 工具 `sbltool` / `tdb` 自动 COM 口发现
- `GETVAR partition-size:<name>` 支持
- `LCD_ENABLED` 可 CMake 覆盖, 无屏模式 SBL 从 48KB → 26KB
- 文档重组 (6 个统一风格文档)

## v3.0 (2026-07-10 ~ 2026-07-11)

- FASTMAP 5 分区落地 (ELF 16K / SBL 48K / REC 64K / TMP 128K / SYSTEM 768K)
- ECDSA P-256 + SHA-256 全链安全启动
- TEE / SAH / USERDATA 分区消灭, 功能并入 ELF / SBL / SD
- ELF 尾部 4KB 状态环形日志 (1→0 直写)
- 热重启加速 (RESTART 标志跳过 ECDSA, 0.5s)
- SBL LCD logo 像素压缩 (110KB → 18KB, 3059 非零像素)
- Flash 设置迁移到 SD 卡 FatFS
- FASTBOOT 菜单 Reboot / Bootloader / Recovery
- 代码风格标准化 (Module_PascalCase, snake_case files)
- `common/` 解耦, linker scripts 移入 `ld/`
- 修复: SBL/TEE PHDR 64KB 对齐跨扇区覆盖
- 修复: Linker gap 0x00 导致签名哈希不匹配, 改用 BIN 烧录
- 修复: `export_partitions.ps1` SYSTEM SignedSize 写错 (0xA0000 → 0xC0000)
- 修复: SD 根目录清理删除 settings.bin (路径移至 `0:/data/`)

## v2.0 (2026-06)

- TOS 分区架构原始实现 (ELF/SBL/TEE/REC/SAH/SYSTEM/USERDATA/TMP)
- 安全启动基础框架 (签名头结构, 向量表检查)
- FASTBOOT USB CDC 协议
- REC SD 卡升级/格式化
- Syslog 分级日志系统

## v1.0 (2026-01)

- 项目初始化和 CubeMX 配置
- SYSTEM 主应用框架
- HAL 驱动层建立
