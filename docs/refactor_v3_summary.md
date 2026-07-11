# TOS Slave-Board v3 重构总结

> 日期：2026-07-10 ~ 2026-07-12
> 目标：安全启动落地 + 分区精简 + SD 卡持久化 + 架构规范化

---

## 1. 分区布局

### 最终布局

| 扇区 | 地址 | 大小 | 分区 | 说明 |
|------|------|------|------|------|
| 0 | `0x00000` | 16 KB | **ELF** | 代码 12KB + 状态记录区 4KB（尾部，1→0 直写） |
| 1-3 | `0x04000` | 48 KB | **SBL** | splash logo（压缩像素）吸入，48KB |
| 4 | `0x10000` | 64 KB | **REC** | 恢复模式 |
| 5 | `0x20000` | 128 KB | **TMP** | 升级暂存，易失 |
| 6-11 | `0x40000` | 768 KB | **SYSTEM** | 含文件系统，SD 卡存设置 |

### 消灭的分区

| 旧分区 | 原因 |
|--------|------|
| TEE | manifest/state 移入 ELF 尾部 4KB |
| SAH | logo 数据压缩后吸入 SBL（3059 像素，18KB） |
| USERDATA | 设置存储迁移到 SD 卡 FatFS |

---

## 2. 安全启动链

```
ELF (不可变，内嵌公钥)
  | ECDSA P-256 + SHA-256 验证 SBL
  v
SBL (签名保护)
  | ECDSA P-256 + SHA-256 验证 SYSTEM / REC
  v
SYSTEM / REC
```

### 重启加速

| 场景 | ELF 行为 | 耗时 |
|------|---------|------|
| 冷启动（上电/RST 按键） | 完整 ECDSA 验证 SBL | ~2s |
| 热启动（`SBL_SystemReboot()`） | 写 RESTART 标志 -> 跳过 ECDSA | ~0.5s |
| 定向重启（`SBL_SystemRebootTo(target)`） | 写 RESTART(target) -> 跳 SBL 后直入目标 | ~0.5s |

### OEM Lock / Unlock

| 状态 | SBL 验证 SYSTEM | FASTBOOT flash |
|------|----------------|----------------|
| Locked | ECDSA 强制 | 写入后当场验签，失败回滚 |
| Unlocked | 仅向量表检查 | 跳过验签 |

---

## 3. 构造变更

### 分区目录架构

```
partitions/
  manifest.h              <- 全局配置、设备 ID、分区布局（唯一事实来源）
  ELF/
    CMakeLists.txt, boot.s
    include/（cust.h, init.h, secure_boot.h, sha256.h, p256-m.h, tee_format.h）
    source/（init.c, cust.c, sha256.c, verify.c, p256-m.c）
  SBL/
    CMakeLists.txt, boot.s
    include/（10 个头文件，去掉 sbl_ 前缀）
    source/（14 个 .c 文件）
  REC/
    CMakeLists.txt, boot.s
    include/
    source/
```

- TEE、SAH 目录已删除
- `common/` 已删除，文件按使用者分散
- `ld/` 统一存放所有 linker script（ELF.ld, SBL.ld, REC.ld, SYSTEM.ld）

### 构建系统

- `cmake/partitions/CMakeLists.txt` 统一 `add_subdirectory` 链接各分区
- `cmake/PartitionTargets.cmake` 定义 `tos_partition_target()` 函数
- SBL 共用文件编译为 `sbl_runtime` OBJECT 库，REC 只 link 不重编
- `image_header.c` 各分区独立拷贝（SBL, REC, SYSTEM），TEE 不再持有

---

## 4. SD 卡设置持久化

### 改动

| 文件 | 内容 |
|------|------|
| `src/core/manager/settings_manager.c` | Flash -> SD：`f_open/f_read/f_write/f_sync` 读写 `0:/settings.bin` |
| `src/core/init.cpp` | SDIO 初始化后立即 `SM_Mount()`；splash 结束后若 SD 缺失弹窗 |
| `Core/Src/main.c` | 移除过早的 `SM_Mount()` 调用 |

### 特性

- `SM_Save()` 无条件写 SD，不依赖 `g_sd_available` 预检
- `SM_Mount()` 对 `FR_NOT_READY` 重试 3 次（延迟 200ms）
- 首次插入无文件时 `FA_CREATE_NEW` 自动创建
- 写入后 `f_sync()` 确保掉电不丢数据
- `debug_log_com` 默认开启（方便串口调试）

---

## 5. FASTBOOT

### 菜单

```
Reboot
Reboot to bootloader
Reboot to recovery
```

### USB 命令

- `FLASH` / `ERASE` / `REBOOT` / `REBOOT RECOVERY` / `REBOOT BOOTLOADER` / `OEM UNLOCK` / `OEM LOCK`
- `getvar` 返回：`product_name`, `version`, `version-bootloader`, `version-baseband`, `serialno`（STM32 96-bit UID）, `unlocked`

---

## 6. 修复的关键 Bug

| Bug | 根因 | 修复 |
|-----|------|------|
| 刷入后 ELF 挂死 | SBL/TEE PHDR 64KB 对齐导致 OpenOCD 跨扇区覆盖 | `max-page-size=0x4000` |
| SYSTEM image mismatch | `export_partitions.ps1` SignedSize 写错（0xA0000） | 修正为 0xC0000 |
| BIN 签名与 ELF 哈希不匹配 | Linker gap 填 0x00，OpenOCD 写 ELF 带入 | 改用 BIN 烧录 + sign_image.ps1 gap-fill |
| SYSTEM image invalid | `manifest.h` 残留旧 `TOS_PART_SYSTEM_SIGNED_SIZE` | 修正为 0xC0000 |
| SD 设置不持久 | `SM_Save` 被 `g_sd_available` 阻塞 | 去预检 + 加 f_sync |
| SD 弹窗每次都出现 | `SM_Init` 过早调 `SM_Mount` | 移到 SDIO init 后 |

---

## 7. 代码风格

- 文件名：`snake_case`
- C 函数：`ModulePrefix_PascalCase()`（如 `ELF_FlashWait`, `SecureBoot_Verify`）
- 静态函数：`snake_case`
- 宏：`UPPER_SNAKE_CASE`
- 枚举类型：`PascalCase_t`（如 `SecureBoot_Result_t`）
- Include 守卫：`FILENAME_H`

---

## 8. 文件变更清单

### 新增
- `partitions/manifest.h` — 全局配置、设备 ID、分区布局
- `partitions/ELF/include/cust.h` / `source/cust.c` — GPIO Bootstrap
- `partitions/SBL/include/logo_data.h` — 压缩 logo 像素（3059 个）
- `cmake/partitions/CMakeLists.txt` — 分区构建聚合
- `ld/` (ELF.ld, SBL.ld, REC.ld, SYSTEM.ld) — 统一 linker script
- `scripts/extract_logo.ps1` — logo 提取工具

### 删除
- `partitions/common/` — 完全解耦
- `partitions/TEE/` — 功能并入 ELF/SBL
- `partitions/SAH/` — logo 吸入 SBL
- `src/hardware/flash_diskio.c/.h` — Flash 块设备驱动

---

## 9. 已知待完善

- REC 的 `tdb.c` / `fat.c` 部分引用旧 USERDATA 宏（已添加兼容别名）
- SYSTEM 分区内文件系统集成（`FMCore_MountStorage` 返回 FR_OK stub）
- 看门狗适配（IWDG 复位走冷启动路径）
