# REC 卡刷升级 + 串口解锁 UI — 修复记录

> 日期: 2026-07-12 | 状态: **串口 BRR Bug 已修复，待验证**

---

## 一、已完成的修复（已验证编译通过）

### 1. `SBL_FlashBegin` 大小检查 (`partitions/SBL/source/flash.c:129`)

```diff
-  if (!session || !part || !part->allow_flash || size != part->size ||
+  if (!session || !part || !part->allow_flash || size > part->size ||
```

签名 BIN 可能小于分区大小（如 REC signed 63KB vs 分区 64KB），`!=` 会错误拒绝。

### 2. `REC_FatFlashUpgrade` 文件大小检查 (`partitions/REC/source/fat.c:680`)

```diff
-  if (!part || image_size != part->size) {
+  if (!part || image_size == 0U || image_size > part->size) {
```

### 3. `ELF_LatestState` PENDING 优先级 (`partitions/ELF/source/init.c:110-114`)

```diff
-    if (TosTeeStateRecordValid(r) && (!latest || r->sequence >= latest->sequence)) {
-      latest = r;
-    }
+    if (!TosTeeStateRecordValid(r)) continue;
+    /* PENDING update always takes priority —
+     * SBL_SystemReboot writes RESTART after PENDING with a higher
+     * sequence, so pure sequence-order would shadow the update. */
+    if (r->txn_state == TOS_TXN_STATE_PENDING) return r;
+    if (!latest || r->sequence >= latest->sequence) latest = r;
```

`SBL_SystemReboot()` 每次复位前无条件写 RESTART 记录（sequence 更高），会遮蔽 PENDING 升级记录。现在 PENDING 无条件优先。

### 4. `ELF_TransactionBounds` SBL 大小检查 (`partitions/ELF/source/init.c:153`)

```diff
-           r->image_size == TOS_PART_SBL_SIZE ? 1U : 0U;
+           r->image_size <= TOS_PART_SBL_SIZE ? 1U : 0U;
```

### 5. ELF 不再支持 REC 自升级

已从 `ELF_SignedImageValid`、`ELF_TransactionBounds`、`ELF_ApplyUpdate` 中移除所有 REC 升级分支。ELF 现在只处理 SBL 的 PENDING 升级。

### 6. `REC_FatFlashUpgrade` 恢复原始版本

移除了之前添加的 REC 自升级两趟逻辑（`rec_path_rec`、`rec_status_rec`、`rec_sbl_update_processed`、`post_boot_target` 参数），恢复为只刷 SYSTEM + SBL。

---

## 二、串口解锁 UI — 已实现但未调通

### 需求

通过 USART1 在控制台同步显示解锁确认界面，支持 GPIOA15 短按切换/长按确认。

### 实现文件

| 文件 | 内容 |
|------|------|
| `partitions/SBL/include/hw.h` (L18-31) | `SBL_UartInit()` / `SBL_UartWrite()` / `SBL_UartWriteText()` / `SBL_UartWriteLine()` 声明，`SBL_UART_ENABLED` 宏（默认 1） |
| `partitions/SBL/source/hw.c` (L79-126) | 裸机 USART1 初始化（PA9=TX AF7, 115200-8N1, BRR=(45<<4)|9, APB2=84MHz） |
| `partitions/SBL/source/init.c` (L183) | `SBL_UartInit()` 在 `SBL_ClockConfig()` 成功后调用（时钟已切到 168MHz） |
| `partitions/SBL/source/ui.c` (L210-217) | 解锁页面入口 — 串口输出标题/正文/YES/NO |
| `partitions/SBL/source/ui.c` (L236,254-268,278-280) | 解锁循环 — 超时/确认/取消/切换时输出状态 |
| `partitions/SBL/source/ui.c` (L350-352) | fastboot 收到 OEM UNLOCK 请求时输出提示 |

### 串口预期输出

```
>>> OEM UNLOCK requested — confirm on device

======== UNLOCK BOOTLOADER ========
Unlock bootloader
This operation will delete all
personal data on your device...
select: short press the button
continue:long press the button
[YES]  NO

 YES  [NO]          ← 短按切换
>>> YES selected — unlocking...    ← 长按确认
OKAY bootloader unlocked

FAIL unlock timeout                ← 30s 超时
```

### 仍未解决：串口无输出

**根本原因已找到：BRR 波特率计算漏掉了因子 16。** 见下方「五、串口修复」。

已排查和尝试：

1. **时钟问题**：最初 `SBL_UartInit()` 在 `SBL_HwBootstrap()` 中调用（系统跑在 16MHz HSI），BRR 按 84MHz APB2 计算，波特率偏差 >5 倍。已移到 `SBL_ClockConfig()` 后（168MHz PLL, APB2=84MHz）。

2. **RX 引脚错误**：最初配置了 PA10 为 RX。`Core/Src/usart.c:105` 显示板子 USART1_RX 实际是 PB7。但只做 TX 输出时 RX 引脚不影响。已移除 PA10 配置，只配 PA9=TX。

3. **兜底初始化**：在 `sbl_draw_unlock_page()` 和 `SBL_UiRunFastboot()` 的 OEM UNLOCK 处理分支中，串口输出前额外调用 `SBL_UartInit()` 作为兜底。

4. **未验证的怀疑点**：
   - USB CDC 初始化 (`SBL_USB_Init`) 是否可能复位或重配 GPIOA 引脚
   - `stm32f4xx_hal_rcc.c` 的 HAL 代码是否在某处覆盖了外设配置
   - 板子 PA9 是否有硬件冲突或被其他外设占用
   - APB2 时钟频率是否真的 84MHz（`SBL_ClockConfig` 设置 `PPRE2_DIV2` 但可能有其他因素）

### 调试建议

1. **用逻辑分析仪/示波器**测量 PA9 引脚，执行 `sbltool oem unlock` 时应有 115200bps 的串口波形。若无波形则 UART 未初始化成功。

2. **用 OpenOCD 手动写 USART1 寄存器**验证硬件通路：
   ```
   # 连接后 halt，手动写 USART1 外设
   openocd ... -c "init" -c "halt"
   # 使能 USART1 时钟
   -c "mww 0x40023844 0x00000010"   # RCC_APB2ENR |= USART1EN
   # 配 PA9 为 AF7
   -c "mww 0x40020000 0xA8000000"   # GPIOA_MODER (bits 18-19 = 10)
   -c "mww 0x40020020 0x00000200"   # GPIOA_AFRH (AFR9 = 7)
   # 配 BRR 和 CR1
   -c "mww 0x40011008 0x000002D9"   # USART1_BRR = 115200@84MHz (USARTDIV=45.57)
   -c "mww 0x4001100C 0x0000200C"   # USART1_CR1 = TE | RE | UE
   # 发一个字节
   -c "mww 0x40011004 0x41"         # USART1_DR = 'A'
   ```
   如果示波器看到波形但终端没收到，检查波特率或 TX/RX 接线。

3. **改用 USART2** (PA2=TX, PA3=RX) 作为备选方案——这两个引脚在 HAL usart.c 中已定义且通常没有冲突。

4. **在 `SBL_UartInit` 返回前加入空转发送**验证函数是否被调用：
   ```c
   // 在 SBL_UartInit() 中 CR1 使能后立即发一个字符
   SBL_UartWrite('U');
   ```

---

## 三、编译与刷写

### 编译

```bash
cd C:\Code\tos\slave-board
cmake -B build/Release -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --target factory_images -j 24
```

产物在 `dist/firmware/`:
- `sbl.bin` (49152 bytes) → 烧录到 0x08004000
- `rec.bin` (65536 bytes) → 烧录到 0x08010000
- `system.bin` (786432 bytes) → 烧录到 0x08040000

### 刷写

VSCode tasks.json 中：
- `Build & Flash`: 全量编译+签名+OpenOCD 烧录 → `scripts/flash.bat factory`
- `Flash System`: 只刷 SYSTEM → `scripts/flash.bat system`
- `Flash Runtime`: 刷 REC+SYSTEM → `scripts/flash.bat runtime`

### 快速测试 SBL 修改

1. `cmake --build build/Release --target factory_images -j 24`
2. 用 OpenOCD 只刷 SBL: 在 OpenOCD 中 `flash write_image erase dist/firmware/sbl.bin 0x08004000`
3. 或者用 FASTBOOT: `fastboot flash sbl dist/firmware/sbl.bin`（需要已解锁 bootloader）

---

## 四、当前代码状态

```
partitions/ELF/source/init.c   — PENDING 优先级 + SBL-only size check，无 REC 升级
partitions/REC/source/fat.c    — 原始单趟刷写，只修复了大小检查
partitions/SBL/source/flash.c  — size > part->size
partitions/SBL/include/hw.h    — UART 声明 (SBL_UART_ENABLED=1)
partitions/SBL/source/hw.c     — USART1 裸机驱动 (PA9 TX)
partitions/SBL/source/init.c   — ClockConfig 后调用 UartInit
partitions/SBL/source/ui.c     — 解锁 UI 串口镜像
partitions/manifest.h          — 用户测试用 LCD_ENABLED=0
```

编译全部通过：sbl.elf 26700 B, rec.elf 51820 B, system.elf 不变。

---

## 五、串口修复 — BRR 波特率计算 Bug

### 根因

`partitions/SBL/source/hw.c:100-102` 的 BRR 计算漏掉了 16 倍过采样的因子。

STM32F4 RM0090 参考手册规定 (OVER8=0, 16× oversampling):

```
BaudRate = f_CK / (16 × USARTDIV)
  ∴ USARTDIV = f_CK / (16 × BaudRate)
```

**错误代码：**
```c
/* 84 000 000 / 115 200 = 729.166... -> mantissa 729, fraction 3/16 */
SBL_UART->BRR = (729UL << 4U) | 3UL;  // BRR = 0x2D93
```

错误：直接用 `f_CK / BaudRate` (84M/115200=729) 而非 `f_CK / (16 × BaudRate)`。

**正确计算：**
```
USARTDIV = 84,000,000 / (16 × 115,200)
         = 84,000,000 / 1,843,200
         = 45.5729...
Mantissa = 45, Fraction = 9/16 (round(0.5729 × 16) = 9)
BRR = (45 << 4) | 9 = 0x02D9
```

### 影响

错误 BRR `0x2D93` → 实际波特率仅 **~7,200 bps**（比预期 115,200 慢 16 倍），接收端完全无法识别。

### 修复

```diff
-  SBL_UART->BRR = (729UL << 4U) | 3UL;
+  SBL_UART->BRR = (45UL << 4U) | 9UL;
```

已同步修正注释和 OpenOCD 调试命令。
