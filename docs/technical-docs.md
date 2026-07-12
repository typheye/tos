# 技术手册

> MCU: STM32F407ZGT6 | Flash: 1 MB | 版本: v3

---

## 1. 分区定义

| # | 扇区 | 地址 | 大小 | 分区 | 功能 | 安全 | 写权限 |
|---|------|------|------|------|------|------|--------|
| 0 | 0 | `0x08000000` | 16 KB | **ELF** | 信任根：验证 SBL + 状态记录 | — | 1→0 直写 (尾部 4KB) |
| 1 | 1-3 | `0x08004000` | 48 KB | **SBL** | 二级引导：验证 SYSTEM/REC + FASTBOOT | ECDSA | 只读 (升级走 TMP) |
| 2 | 4 | `0x08010000` | 64 KB | **REC** | 恢复模式：SD 升级 / 格式化 | ECDSA | 只读 |
| 3 | 5 | `0x08020000` | 128 KB | **TMP** | 升级暂存区 (易失) | — | 任何阶段 |
| 4 | 6-11 | `0x08040000` | 768 KB | **SYSTEM** | 主固件 + 文件系统 | ECDSA | 任何阶段 |

**ELF 内部分布**: 前 12 KB 代码 + 后 4 KB 状态记录区 (64 条 × 64B 环形日志)

---

## 2. 启动流程

```
上电/硬复位 (冷启动):
  ELF Reset_Handler -> Cust_Setup(GPIO) -> ECDSA 验证 SBL(~2s)
  -> ELF_Jump(SBL)
  SBL -> splash -> 验证 SYSTEM -> Jump(SYSTEM)

软复位 (SBL_SystemReboot):
  ELF -> 检测 RESTART 记录 -> 直接 Jump(SBL) (~0.5s)

定向重启 (SBL_SystemRebootTo):
  ELF -> 检测 RESTART(boot_target) -> Jump(SBL) -> SBL 跳指定目标

OEM UNLOCK 流程:
  sbltool oem unlock -> 确认提示 -> SBL: SetUnlocked(1)
  -> boot_target=RECOVERY_FORMAT -> 重启 -> REC: 格式化SD+userdata -> 重启
```

### boardLED 状态指示

无 LCD 模式下通过 GPIOC13 快速判断当前阶段：

| 阶段 | LED 状态 |
|------|----------|
| FASTBOOT | 常亮 |
| SYSTEM 损坏 | 快闪 (100ms 周期) → 5s 后自动重启 |
| REC 模式 | 半秒闪烁 (500ms 周期) |
| REC 损坏异常 | 5s 后自动重启 |

### 状态记录格式

```c
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;           // TOS_TEE_STATE_MAGIC
  uint32_t version;         // TOS_TEE_STATE_VERSION
  uint32_t sequence;        // 全局递增序号
  uint32_t unlocked;        // 0=locked, 1=unlocked
  uint32_t boot_target;     // FASTBOOT / RECOVERY / NONE
  uint32_t update_kind;     // SBL / REC / NONE
  uint32_t txn_state;       // PENDING / DONE / FAILED / RESTART
  uint32_t source_address;  // TMP
  uint32_t target_address;  // SBL_ADDR / REC_ADDR
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t post_boot_target;
  uint32_t reserved[3];
  uint32_t record_crc;
} TosTeeStateRecord;  // 64 字节
```

---

## 3. 签名机制

### TosImageHeader (128B, 位于分区偏移 0x200)

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | magic = `0x31474953` ("SIG1") |
| 4 | 4 | header_version = 1 |
| 8 | 4 | image_type (1=SBL, 2=REC, 3=SYSTEM) |
| 12 | 4 | load_address |
| 16 | 4 | image_size |
| 20 | 4 | image_version |
| 24 | 4 | flags |
| 28 | 32 | SHA-256 digest |
| 60 | 64 | ECDSA P-256 signature (r||s) |
| 124 | 4 | header_crc32 |

### 签名流程 (PC)

```bash
# 生成密钥 (一次性)
openssl ecparam -name prime256v1 -genkey -noout -out tos_privkey.pem

# 构建自动签名
cmake --build build/Release --target factory_images
# → sign_image.ps1 自动:
#   1. objcopy ELF -> BIN (--gap-fill 0xFF --pad-to)
#   2. SHA-256 全镜像 (header 占位 0xFF)
#   3. ECDSA P-256 签名
#   4. 写入 TosImageHeader
#   5. verify_image.ps1 自检
```

---

## 4. 移植指南

### 4.1 移植到同系列 MCU (STM32F4xx)

**核心原则**：只改 `manifest.h` + linker scripts。所有分区代码与硬件解耦。

**步骤**:

1. **分区布局** — `partitions/manifest.h`:
   ```c
   #define TOS_FLASH_SIZE  0x00080000UL  // 改为实际 Flash 大小
   // 调整 TOS_PART_* 宏
   ```

2. **Linker scripts** — `ld/*.ld`:
   同步修改 `FLASH ORIGIN` / `LENGTH`

3. **按键/外设** — `partitions/ELF/source/cust.c`:
   `Cust_Setup()` 中修改 GPIO 初始化

4. **LCD** — `partitions/manifest.h`:
   `#define LCD_ENABLED 0` 禁用

5. **USB** — 修改 `USB_DEVICE` 描述符 (VID/PID)

### 4.2 移植到不同架构 MCU

1. `startup_stm32f407xx.s` → 替换为目标 MCU 的启动文件
2. `Core/Src/system_stm32f4xx.c` → 替换系统时钟配置
3. `Drivers/` → 替换 HAL/CMSIS 库
4. `cmake/gcc-arm-none-eabi.cmake` → 可能调整编译器/链接器参数

### 4.3 添加新分区

1. `partitions/manifest.h` — 添加 `TOS_PART_*` 宏
2. `ld/` — 创建新的 linker script
3. `cmake/partitions/CMakeLists.txt` — `add_subdirectory`
4. 分区自身 — `CMakeLists.txt` + `source/` + `include/` + `boot.s`
5. `scripts/export_partitions.ps1` — 添加 Export-Partition 行

### 4.4 更换签名密钥

```bash
# 删除旧密钥
rm sign/development-signing-key.pk8*
# 重新配置 CMake (自动生成新密钥)
cmake --preset Release
# 重编译
cmake --build build/Release --target factory_images
```

---

## 5. 调试

### 串口日志

`debug_log_com` 默认开启，USART1 (PA9/PA10) 115200-8N1。

### OpenOCD

```bash
# 读 PC
openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
  -c "init" -c "halt" -c "reg pc" -c "shutdown"

# 读 Flash 内存
openocd ... -c "mdw 0x08004200 8" -c "shutdown"
```

### FASTBOOT

```bash
# 设备探测
sbltool devices

# 读分区表
sbltool getvar all

# 读序列号
sbltool getvar serialno
```

---

## 6. 约束

| 项目 | 限制 |
|------|------|
| ELF 代码 | ≤ 12 KB |
| SBL 总大小 | ≤ 48 KB |
| REC 总大小 | ≤ 63 KB (尾部 1KB 命令区) |
| SYSTEM 总大小 | ≤ 768 KB |
| TEE 状态记录 | 环形 64 条 × 64B = 4 KB |
| 签名算法 | ECDSA P-256 only |
| 烧录方式 | BIN (不用 ELF，避免 gap-fill 问题) |

---

## 7. 关键配置宏 (`manifest.h`)

| 宏 | 说明 | 默认值 |
|----|------|--------|
| `TOS_PRODUCT_NAME` | 产品名 | `"CNAEK7"` |
| `TOS_HW_REV` | 硬件版本 | `"V1"` |
| `LCD_ENABLED` | LCD 启用 | `1` |
| `BTN_PORT` / `BTN_PIN` | 按键引脚 | `GPIOA` / `15U` |
| `TOS_USB_VID` / `TOS_USB_PID` | USB 标识 | `0xFAED` / `0x4870` |
| `TOS_BL_VERSION` | Bootloader 版本 | `"TOSV1.0"` |

---

## 8. 常见问题

**Q: 刷入后开不了机?**

A: 确保用 `dist/firmware/*.bin` 烧录 (非 `.elf`)，且全量 `factory_images` 构建通过。

**Q: "System image invalid"?**

A: 检查 `manifest.h` 中 `TOS_PART_SYSTEM_SIGNED_SIZE` 是否匹配实际分区大小。

**Q: SD 设置不保存?**

A: 检查 `SM_Mount()` 是否在 SDIO 初始化后调用；确认 `0:/data/settings.bin` 路径正确。

**Q: 如何关闭 LCD?**

A: `manifest.h` 设 `#define LCD_ENABLED 0`，重新构建。SBL 体积降至 ~26 KB。
