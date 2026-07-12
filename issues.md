# REC 卡刷升级无法写入 SBL/REC 分区 — 问题追踪

> 发现日期: 2026-07-12 | 状态: **已修复** (2026-07-12)

---

## 1. 现象

REC 模式下从 SD 卡选择 `sbl.bin` 或 `rec.bin` 执行升级，REC 提示"Finished"并重启，但启动后仍是旧固件，分区未被实际更新。

## 2. 升级链路（预期行为）

```
REC 模式
  │ 读 SD 卡 sbl.bin/rec.bin
  │ 逐块写入 TMP (0x08020000, 128KB)
  │ SBL_FlashFinalize → SBL_StateScheduleUpdatePost
  │ 写 TEE 状态记录 (PENDING, target=SBL_ADDR/REC_ADDR)
  ▼
重启
  │
ELF
  │ ELF_LatestState() → 发现 PENDING
  │ ELF_ApplyUpdate():
  │   CRC32 校验 TMP 镜像
  │   ELF_SignedImageValid(TMP → 验 ECDSA 签名)
  │   擦目标扇区 (SBL: 1-3, REC: 4)
  │   逐字复制 TMP → target
  │   CRC32 + ECDSA 再验 target
  │   ELF_MarkState(DONE)
  ▼
重启 → SBL → 新固件
```

## 3. v3 分区变更可能影响的点

| 变更 | 影响 |
|------|------|
| TMP 从 sector 10 → sector 5 (0x080C0000 → 0x08020000) | `SBL_FlashPartition` 表中 TMP staging 地址是否正确 |
| SBL 32KB → 48KB | `TOS_PART_SBL_SIZE` 宏变更，签名 BIN 大小 48KB |
| REC signed size 63KB（分区 64KB） | `fat.c:680` 用 `part->size` 比较文件大小 → 不匹配 (已修复) |
| TEE 状态区从 0x0800F000 → 0x08003000 | `SBL_StateScheduleUpdatePost` 写 TEE 的地址可能错 |
| TEE 已删除 | REC 代码可能引用旧 TEE 地址 |

## 4. 已排查的点

### 4.1 SBL_FlashPartition 表 (`flash.c:20-29`)

```c
{part_sbl, ..., TOS_TMP_STAGE_ADDRESS, TOS_PART_SBL_SIZE, 1U, 1U, 0U, 0U},
{part_rec, ..., TOS_TMP_STAGE_ADDRESS, TOS_PART_REC_SIZE, 1U, 1U, 1U, 0U},
```

`TOS_TMP_STAGE_ADDRESS = 0x08020000` ✓
`TOS_PART_SBL_SIZE = 0xC000` ✓
`TOS_PART_REC_SIZE = 0x10000` ✓

### 4.2 ELF_ApplyUpdate (`ELF/init.c:213-253`)

- 擦除 SBL: sector 1,2,3 (各 16KB = 48KB) ✓
- 擦除 REC: sector 4 (64KB) ✓
- 字数: `r->image_size / 4U` — 取决于状态记录的 `image_size`
- 双重验证: TMP 验签 + target 验签 ✓

### 4.3 REC 文件大小检查 (`fat.c:678-686`)

```c
part = SBL_FlashFindPartition(part_name);
image_size = (uint32_t)f_size(&file);
if (!part || image_size == 0U || image_size > part->size) {  // ← 已修复
```

修复: `image_size != part->size` → `image_size > part->size`。REC 签名 BIN 63KB < 分区 64KB。

### 4.4 状态记录写入 (`state.c:54-85`)

```c
r.target_address = target_address;  // SBL_ADDR or REC_ADDR
r.image_size = image_size;          // session->expected_size
r.source_address = session->part->staged ? TOS_TMP_STAGE_ADDRESS : target_address;
```

`SBL_StateScheduleUpdatePost` 从 `SBL_FlashFinalize` 获取参数。`session->expected_size` 来自 REC 的 `image_size` (f_size 值)。

### 4.5 SBL_FlashBegin 大小限制 (`flash.c:126-143`)

```c
if (!part || size == 0U || size > part->size || expected_crc == 0xFFFFFFFFUL)
    return 0U;
if (part->staged && size > TOS_TMP_STAGE_SIZE)
    return 0U;
```

对于 REC: `size=0xFC00`, `part->size=0x10000` → pass
对于 SBL: `size=0xC000`, `part->size=0xC000` → pass

## 5. 可疑但未验证的点

### 5.1 TEE 状态区地址混乱

TEE 状态区已从原 `0x0800F000` 移到 `0x08003000`。REC 可能通过 SBL 代码写状态记录时仍引用旧宏。

**验证方法**: 在 REC 闪完后不要重启，用 OpenOCD 读 `0x08003000` 区，确认有 PENDING 记录。

```bash
openocd ... -c "mdw 0x08003000 16" -c "shutdown"
```

期望: 第一条记录 magic=`0x54535431`, txn_state=`0xFFFFFFFE`(PENDING)

### 5.2 ELF_TransactionBounds 拒绝

`ELF_TransactionBounds` 检查状态记录的 source/target 地址是否合法。如果 source 不是 TMP 地址或 target 不是 SBL/REC 地址，返回 0。

**检查代码**:
```c
static uint8_t ELF_TransactionBounds(const TosTeeStateRecord *r) {
  if (r->source_address != TOS_TMP_STAGE_ADDRESS) return 0U;
  if (r->target_address != TOS_PART_SBL_ADDRESS &&
      r->target_address != TOS_PART_REC_ADDRESS) return 0U;
  ...
}
```

### 5.3 签名验证失败

ELF 在 TMP 上调用 `ELF_SignedImageValid` 验证签名。如果 TMP 中的数据损坏（比如 REC 写入后没 flush cache），签名可能不匹配。

**验证方法**: ELF 启动前 dump TMP 区对比 SD 卡文件:

```bash
openocd ... -c "dump_image tmp_dump.bin 0x08020000 0x20000" -c "shutdown"
cmp tmp_dump.bin sbl_signed.bin
```

### 5.4 SD 卡文件被根目录清理脚本删除

`cleanup_sd_root_whitelist()` 可能在 REC 升级前清理了 SD 根目录的文件。

## 6. 调试技巧

### 6.1 串口日志

`debug_log_com=true`(默认), USART1 115200-8N1。REC 模式会输出升级进度。ELM_ApplyUpdate 失败时会打印原因。

### 6.2 OpenOCD 断点

```bash
# 在 ELF_ApplyUpdate 开头设断点
openocd ... -c "bp 0x0800XXXX 2 hw" -c "resume"
```

### 6.3 手动验证升级

1. 用 FASTBOOT 刷一次 SBL (这路径已验证可行)
2. 对比 FASTBOOT 和 REC 的 TEE 状态记录是否一致
3. 用 OpenOCD 在 ELF 启动后直接读 TEE 状态区

### 6.4 强制重启前检查 TEE

REC 升级后不要立即重启，halt 设备:
```bash
openocd ... -c "init" -c "halt"
# 此时 REC 已写完 TMP 和 TEE 状态
-c "mdw 0x08020000 8"   # TMP 头 32 字节
-c "mdw 0x08003000 16"   # 状态记录
-c "resume"
```

## 7. 文件变更清单 (v3)

| 文件 | 变更 |
|------|------|
| `partitions/manifest.h` | 分区布局全量重写, TEE/SAH/USERDATA 删除 |
| `partitions/SBL/source/flash.c` | 分区表从 7 项 → 4 项 |
| `partitions/SBL/source/state.c` | `SBL_StateSetRestart(boot_target)` |
| `partitions/SBL/source/hw.c` | `SBL_SystemRebootTo`, 移除 Shutdown |
| `partitions/SBL/source/splash.c` | SAH → 压缩像素 |
| `partitions/SBL/source/ui.c` | SYSTEM DAMAGE 按原因分类, FASTBOOT 菜单 3 项 |
| `partitions/SBL/source/usb.c` | GETVAR serialno, REBOOT BOOTLOADER |
| `partitions/ELF/source/init.c` | ELF_Main 重写, 状态区在 ELF 尾部, GPIO 移入 Cust_Setup |
| `partitions/ELF/source/cust.c` | 新增 GPIO Bootstrap |
| `partitions/REC/source/fat.c` | 文件大小检查修复 (line 680) |
| `src/core/manager/settings_manager.c` | Flash → SD 持久化 |
| `src/core/init.cpp` | SM_Mount 时序修复 |
| `src/hardware/include/sfhd.h` | 从 git 恢复 + manifest 引用更新 |

---

## 8. 根因与修复 (2026-07-12)

### 根因：两个串行 bug，第一个立即阻断，第二个是后续阻塞

#### Bug #1 — `SBL_FlashBegin` 拒绝 REC (`flash.c:129`)

`SBL_FlashBegin` 校验 `size != part->size`。签名后的 `rec.bin` = 0xFC00 (63KB)，分区 `TOS_PART_REC_SIZE` = 0x10000 (64KB)。不匹配 → 返回 0。
REC 的 fat.c:680 已从 `!=` 改为 `>`，但 `SBL_FlashBegin` 漏改。

**影响**: `SBL_FlashBegin` 直接返回 0，数据从未写入 TMP，`SBL_FlashFinalize` 从未调用，状态记录从未创建。

**修复** (`flash.c:129`):
```diff
-  if (!session || !part || !part->allow_flash || size != part->size ||
+  if (!session || !part || !part->allow_flash || size > part->size ||
```

---

#### Bug #2 — `ELF_TransactionBounds` 拒绝 REC (`init.c:150, 154`)

状态记录的 `image_size` = 实际文件大小 (0xFC00)。ELF 重启后 `ELF_TransactionBounds` 做完整性检查：

```c
r->image_size == TOS_PART_REC_SIZE  // 0xFC00 == 0x10000 → false → 拒绝
```

即使 Bug #1 修复，ELF 仍会拒绝并将交易标为 FAILED。

**修复** (`init.c:150, 154`):
```diff
-           r->image_size == TOS_PART_SBL_SIZE ? 1U : 0U;
+           r->image_size <= TOS_PART_SBL_SIZE ? 1U : 0U;
-           r->image_size == TOS_PART_REC_SIZE ? 1U : 0U;
+           r->image_size <= TOS_PART_REC_SIZE ? 1U : 0U;
```

---

### 修复后完整链路

| 步骤 | 位置 | 内容 | 结果 |
|------|------|------|------|
| 1 | fat.c:680 | `image_size (0xFC00) > part->size (0x10000)`? No → pass | OK |
| 2 | flash.c:129 | `size (0xFC00) > part->size (0x10000)`? No → pass | **OK (fixed)** |
| 3 | flash.c:138 | `session->expected_size = 0xFC00` | OK |
| 4 | flash.c:224 | `SBL_StateScheduleUpdatePost(REC, REC_ADDR, 0xFC00, ...)` | OK |
| 5 | state.c:77 | State record: `image_size=0xFC00` at `0x08003000+off` | OK |
| 6 | Reboot → ELF | `ELF_TransactionBounds`: `0xFC00 <= 0x10000`? Yes | **OK (fixed)** |
| 7 | init.c:225 | Erase sector 4 (64KB) | OK |
| 8 | init.c:237 | Copy TMP→REC (0xFC00 bytes) | OK |
| 9 | init.c:242 | CRC32 + ECDSA verify target | OK |
| 10 | init.c:247 | `ELF_MarkState(DONE)` | OK |
| 11 | Reboot → SBL → 新固件 | | OK |

### 编译验证 ✓

```
sbl.elf:  48068 B / 48 KB (97.79%)
rec.elf:  53776 B / 63 KB (83.36%)
sbl.bin:  49152 bytes (signed)
rec.bin:  64512 bytes (signed)
```

---

## 9. Bug #3 — RESTART 记录遮蔽 PENDING (2026-07-12)

### 现象

REC 刷写完成后第一次重启，**旧 SBL 仍然运行**（logo 亮起），然后再次复位才运行新 SBL。

### 根因

`ELF_LatestState()` 按 `sequence` 取最大值。升级链路中存在两条记录的竞态：

| 时序 | 谁写的 | 记录类型 | sequence |
|------|--------|----------|----------|
| 1 | `SBL_FlashFinalize` → `SBL_StateScheduleUpdatePost` | PENDING (SBL) | N |
| 2 | `SBL_SystemReboot()` → `SBL_StateSetRestart` | RESTART | **N+1** |

`SBL_SystemReboot()` 每次复位前都会无条件写入 RESTART 记录，其 sequence 比 PENDING 高 1。ELF 启动后 `ELF_LatestState()` 返回 RESTART（sequence 更高），`ELF_Main` 检查到 RESTART 状态就直接跳转到 SBL — **完全绕过 PENDING 处理**。

```
ELF_Main:
  │ state = ELF_LatestState()  → 返回 RESTART (seq=N+1)
  │ if (state->txn_state == RESTART) → TRUE
  │   ELF_MarkState(CONSUMED)
  │   ELF_Jump(SBL)  → 旧 SBL！
  ▼
旧 SBL 运行（亮 logo）
  │ ...最终某个路径触发复位...
  ▼
ELF 再次启动
  │ RESTART 已失效 (txn_state=0xFFFFFFE0 → TosTeeStateRecordValid=false)
  │ ELF_LatestState() → 返回 PENDING (seq=N)
  │ ELF_ApplyUpdate → 刷写 → ELF_Reset
  ▼
ELF 第三次启动 → 跳转新 SBL（黑屏）
```

### 修复

`ELF_LatestState()` — PENDING 记录无条件优先返回，忽略 sequence 顺序：

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

### 修复后链路

```
REC 刷 SYSTEM + SBL → SBL_SystemReboot (写 RESTART) → reset
  │
ELF (第一次):
  │ ELF_LatestState() → 返回 PENDING (优先级高于 RESTART)
  │ ELF_ApplyUpdate → 擦 sector 1-3 → 写 TMP→SBL → 验证 → Mark DONE
  │ ELF_Reset()
  ▼
ELF (第二次):
  │ PENDING 现在是 DONE → 跳过
  │ RESTART 仍然是 RESTART → ELF_Jump(SBL)
  ▼
SBL (新固件, LCD_ENABLED=0) → 黑屏 → TOS 开机动画 ✓
```

**只需一次 SBL 启动，且启动的是新固件。**

---

## 10. Bug #4 — REC 分区从未被 REC 卡刷升级 (2026-07-12)

### 现象

REC 升级界面依次显示 "Flashing SYSTEM...", "Flashing SBL..."，**从未显示 "Flashing REC..."**。`REC_FatFlashUpgrade` 只刷 TEE、SAH、SYSTEM、SBL，不刷 REC 自身。

### 根因

SBL 和 REC 都通过 TMP 中转升级，但 TMP 是 STM32F4 的单个 128KB 扇区（sector 5），不能部分擦除。同时放两
个镜像会被后一个擦除。所以需要**两趟升级**：

- **第一趟**: 刷 SBL 到 TMP → PENDING for SBL → 重启 → ELF 应用 → SBL 重新进入 REC
- **第二趟**: 刷 REC 到 TMP → PENDING for REC → 重启 → ELF 应用 → 正常启动

### 修复

**`fat.c`**:

1. 新增 `rec_path_rec`、`rec_status_rec` 常量
2. `rec_flash_file` 新增 `post_boot_target` 参数，对 `staged` 分区统一设置（不再只针对 SBL 硬编码 NONE）
3. 新增 `rec_sbl_update_processed()` — 扫描 TEE 状态区，若已有 SBL DONE/FAILED 记录则返回 1（说明已进入第二趟）
4. `REC_FatFlashUpgrade` 三路分支：
   - **有 rec.bin 且 SBL 已处理** → 第二趟：只刷 REC，`post_boot_target = NONE`
   - **第一趟**: 刷 TEE/SAH/SYSTEM/SBL，SBL 的 `post_boot_target` 根据 rec.bin 是否存在：
     - `rec.bin` 存在 → `TOS_BOOT_TARGET_RECOVERY_UPGRADE`（链式进入第二趟）
     - `rec.bin` 不存在 → `TOS_BOOT_TARGET_NONE`（单趟完成）

### 完整升级链路（sbl.bin + rec.bin 都存在）

```
REC 第一趟
  │ 刷 SYSTEM (直接烧写)
  │ 刷 SBL → TMP → PENDING SBL (post_boot=RECOVERY_UPGRADE)
  │ SBL_SystemReboot → RESTART → reset
  ▼
ELF (第一次)
  │ ELF_LatestState → PENDING SBL (优先级 > RESTART)
  │ ELF_ApplyUpdate → 擦 sector 1-3 → TMP→SBL → DONE
  │ ELF_Reset
  ▼
ELF (第二次)
  │ ELF_LatestState → RESTART → ELF_Jump(SBL)
  ▼
SBL (新固件)
  │ SBL_StatePeekBootTarget → RECOVERY_UPGRADE → 进入 REC
  ▼
REC 第二趟
  │ rec_sbl_update_processed() → TRUE
  │ 刷 REC → TMP → PENDING REC (post_boot=NONE)
  │ SBL_SystemReboot → RESTART2 → reset
  ▼
ELF (第三次)
  │ ELF_LatestState → PENDING REC (优先级 > RESTART2)
  │ ELF_ApplyUpdate → 擦 sector 4 → TMP→REC → DONE
  │ ELF_Reset
  ▼
ELF (第四次)
  │ ELF_LatestState → RESTART2 → ELF_Jump(SBL)
  ▼
SBL (新固件) → 正常启动 → TOS ✓
```

### 编译验证

```
rec.elf:  54084 B / 63 KB (83.84%)  (+308 bytes vs 修复前)
sbl.elf:  48068 B / 48 KB (97.79%)
```
