# TOS Slave-Board 安全启动架构规范

> **版本**：v3（集成 TEE 升级方案 + 统一状态记录 + REC 命令迁移）
> **平台**：STM32F407ZGT6 | 1MB Flash | 192KB RAM
> **状态**：工厂测试阶段，尚无已部署设备，布局可自由调整

---

## 1. Flash 分区布局

### 1.1 扇区地图

| 扇区 | 偏移 | 大小 | 分区 | 职责摘要 |
|------|------|------|------|----------|
| 0 | `0x00000` | 16 KB | **ELF** | 信任根，不可变，含公钥+AES密钥 |
| 1-2 | `0x04000` | 32 KB | **SBL** | 二级引导，验证 SYSTEM/REC |
| 3 | `0x0C000` | 16 KB | **TEE** | 前12KB Manifest（签名保护）+ 后4KB 统一状态记录区 |
| 4 | `0x10000` | 64 KB | **REC** | 恢复模式，SD/USB 升级 |
| 5 | `0x20000` | 128 KB | **SAH** | splash logo 等资源数据 |
| 6 | `0x40000` | 128 KB | **TMP** | 升级暂存区（加密存储） |
| 7 | `0x60000` | 128 KB | **USERDATA** | 用户设置 + FAT 卷（加密存储） |
| 8-11 | `0x80000` | 512 KB | **SYSTEM** | 主应用固件 |

**设计意图**：低地址（0-5）放系统/启动/安全分区，中地址（6-7）放可变/加密数据，高地址（8-11）放最大的主应用分区。

### 1.2 分区属性和完整访问控制矩阵

| 分区 | 大小 | 签名保护 | 加密 | **ELF 下** | **SBL 下** | **REC 下** | 备注 |
|------|------|---------|------|-----------|----------|----------|------|
| **ELF** | 16KB | ✅ 签名强制（自身不可变） | — | — | 只读 | 只读 | 物理信任根 |
| **SBL** | 32KB | ✅ 受签名保护 | — | 可擦写（更新时） | 可刷写 **不可擦除** | 可刷写 **不可擦除** | 防降级 |
| **TEE** | 16KB | ✅ 前12KB受签名保护 | — | **可擦写（仅升级事务）** | 只读 | 只读 | 状态区由所有分区读写 |
| **REC** | 64KB | ✅ 受签名保护 | — | 可擦写（更新时） | 可刷写可擦除 | 可刷写 **不可擦除** | REC 不能擦自己 |
| **SAH** | 128KB | — | — | — | 可刷写可擦除 | 可刷写可擦除 | 资源数据 |
| **TMP** | 128KB | — | ✅ AES-128-CBC | 可读 | 可刷写可擦除 | 可刷写可擦除 | 升级暂存 |
| **USERDATA** | 128KB | — | ✅ AES-128-CBC | — | 可刷写可擦除 | 可刷写可擦除 | 用户数据卷 |
| **SYSTEM** | 512KB | ✅ 受签名保护 | — | — | 可刷写可擦除 | 可刷写可擦除 | 主应用 |

> **ELF 下权限**列：只标注了 ELF 参与的分区更新操作。其他分区 ELF 不涉及。
>
> TEE 的 "只读" 限制适用于 SBL 和 REC。ELF 作为信任根，在受控的升级事务中可以擦写 TEE 整扇区——详见第 5 章。

---

## 2. 安全启动信任链

### 2.1 架构

```
                      ELF (不可变信任根)
                     ┌──────────────────┐
                     │  内嵌根公钥(64B)  │
                     │  内嵌AES密钥(16B) │
                     └────────┬─────────┘
                    ┌─────────┼─────────┐
                    ▼         ▼         ▼
              ┌──────────┐ ┌──────┐ ┌──────┐
              │ 验证 SBL  │ │ 升级  │ │ 升级  │
              │  ECDSA   │ │ TEE  │ │SBL/REC│
              └────┬─────┘ └──────┘ └──────┘
                   │ 通过后跳转
                   ▼
              ┌──────────┐
              │   SBL    │
              │ 验证     │
              │ SYSTEM   │──→ SYSTEM
              │ REC      │──→ REC
              │ FASTBOOT │──→ USB 命令
              └──────────┘
```

### 2.2 逐级验证职责

| 步骤 | 执行者 | 验证目标 | 验证内容 |
|------|--------|---------|---------|
| ① | ELF | SBL 分区 | SHA-256 → ECDSA P-256 签名 → 版本号 ≥ min_sbl_version |
| ② | SBL | SYSTEM 分区 | 同上（公钥从 ELF 区域直接读，不信任 TEE） |
| ③ | SBL | REC 分区 | 同上（仅在需要跳转 REC 时执行） |
| ④ | SBL (锁定态) | FASTBOOT flash 写入 | 写入完成后当场验证签名，失败则回滚 |

### 2.3 验证失败策略

| 场景 | 行为 |
|------|------|
| SBL 签名无效 | ELF 停止启动，LED 错误码 |
| SYSTEM 签名无效 | SBL 尝试跳 REC（若 REC 签名有效） |
| REC 签名无效 | SBL 显示 "SYSTEM DAMAGE"，停止 |
| 版本号过低（回滚） | 拒绝启动，可尝试 REC 恢复 |

---

## 3. 密码学参数

| 参数 | 值 | 用途 |
|------|---|------|
| 签名曲线 | NIST P-256 (secp256r1) | ECDSA 签名验证 |
| 公钥格式 | 未压缩 (X\|\|Y) 64 字节 | 嵌入 ELF |
| 签名格式 | r\|\|s 原始 64 字节 | TosImageHeader 中存储 |
| 哈希算法 | SHA-256 | 镜像摘要 |
| 对称加密 | AES-128-CBC | TMP + USERDATA 加密 |
| 对称密钥 | 16 字节，嵌入 ELF `.rodata` | 所有设备共用（或 UID 派生） |
| 密钥派生（可选） | SHA-256(UID_96bit \|\| 固定盐) | 设备绑定密钥 |

---

## 4. 分区头部结构

每个签名分区（SBL、REC、SYSTEM、TEE 前 12KB）头部偏移 `0x200` 处放置以下结构：

```c
#define TOS_IMAGE_HEADER_MAGIC    0x544F5331UL  /* "TOS1" */
#define TOS_IMAGE_HEADER_OFFSET   0x200UL

typedef struct __attribute__((packed)) {
    uint32_t magic;               /* TOS_IMAGE_HEADER_MAGIC */
    uint32_t header_version;      /* 结构版本 = 1 */
    uint32_t image_length;        /* 实际镜像长度（不含 header、不含填充） */
    uint32_t image_crc32;         /* 快速损坏检测 */
    uint8_t  sha256_digest[32];   /* 镜像 SHA-256 */
    uint8_t  ecdsa_signature[64]; /* ECDSA P-256 (r||s) */
    uint32_t image_version;       /* 版本号（防回滚） */
    uint32_t flags;               /* 预留 */
    uint32_t reserved[14];        /* 填充至 128 字节 */
} TosImageHeader;
```

**说明**：向量表占用 `0x000-0x1FF`，header 放在 `0x200-0x27F`，实际代码从 `0x280` 开始。TEE Manifest 使用 `TosTeeManifest` 而不是 `TosImageHeader`（结构不同，详见第 5 章）。

---

## 5. TEE 分区（核心枢纽）

TEE 是整个安全系统的统一通信枢纽——所有分区之间的升级事务、启动目标、锁定状态、恢复命令**全部通过 TEE 后 4KB 的状态记录区传递**，单一事实来源。

### 5.1 分区布局

```
TEE 分区 (16KB, 扇区 3)
┌─────────────────────────────────────────────────────────┐
│ 偏移 0x0000 ~ 0x2FFF (12KB) — Manifest 区               │
│ ┌───────────────────────────────────────────────────┐   │
│ │ TosTeeManifest                                     │   │
│ │ · magic + version                                  │   │
│ │ · 分区表 (8个分区, offset, size, flags)              │   │
│ │ · public_key_hash[32] — SHA-256(根公钥)              │   │
│ │ · min_sbl_version / min_rec_version                │   │
│ │ · min_system_version / security_flags              │   │
│ │ └───────────────────────────────────────────┘      │   │
│ │                                                     │   │
│ │ 签名图片头 TosImageHeader @ 0x200                    │   │
│ │ → 前 12KB 作为一个整体被 ECDSA 签名保护              │   │
│ └───────────────────────────────────────────────────┘   │
│                                                         │
│ 偏移 0x3000 ~ 0x3FFF (4KB) — 统一状态记录区              │
│ ┌───────────────────────────────────────────────────┐   │
│ │ TosTeeStateRecord 环形日志                         │   │
│ │ (64 字节/条，最多 64 条)                            │   │
│ │                                                     │   │
│ │ 记录类型 (type 字段):                                │   │
│ │ · BOOT_TARGET  — 下次启动目标                        │   │
│ │ · LOCK_STATE   — 锁定/解锁状态切换                   │   │
│ │ · UPGRADE_TXN  — SBL/REC/TEE 升级事务               │   │
│ │ · COMMAND      — 分区间命令（如 REC 格式化）          │   │
│ │ · POST_ACTION  — 升级完成后的后续动作                 │   │
│ └───────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
         ↑                  ↑
     受 ECDSA 签名保护    运行时自由读写（不签名）
```

**关键约束**：

- **前 12KB**：只在 TEE 升级事务中被 ELF 整片擦写，其余时间**所有分区只读**
- **后 4KB**：ELF、SBL、REC、SYSTEM **均可写入**（append-only 环形日志），不参与签名校验
- **擦除粒度**：整个扇区 3（16KB），擦除由 ELF 在升级事务中执行

### 5.2 Manifest 结构

```c
// 增强后的 Manifest（兼容现有 magic/version）
typedef struct __attribute__((packed)) {
    uint32_t magic;                     /* TOS_TEE_MANIFEST_MAGIC */
    uint32_t version;                   /* TOS_TEE_MANIFEST_VERSION */
    uint32_t partition_count;           /* 8 */
    uint32_t flags;
    TosTeePartition partitions[8];      /* 分区描述表 */
    uint8_t  public_key_hash[32];       /* SHA-256(根公钥) */

    /* ↓ 扩展（复用原 reserved[64] 的前 24 字节） */
    uint32_t min_sbl_version;           /* 最低 SBL 版本 */
    uint32_t min_rec_version;           /* 最低 REC 版本 */
    uint32_t min_system_version;        /* 最低 SYSTEM 版本 */
    uint32_t security_flags;            /* 安全配置位 */
    uint32_t reserved_tee_manifest[12]; /* 剩余 48 字节 */
} TosTeeManifest;
```

### 5.3 统一状态记录结构

所有分区间的数据交换都通过同一种记录格式，用 `type` 字段区分语义：

```c
#define TOS_TEE_STATE_MAGIC    0x54535431UL  /* "TST1" */
#define TOS_TEE_STATE_VERSION  2UL            /* v2: 新增 type 字段 */

/* 记录类型 */
#define TOS_RECORD_TYPE_BOOT_TARGET  0x01  /* 下次启动到哪个分区 */
#define TOS_RECORD_TYPE_LOCK_STATE   0x02  /* unlock/lock 切换 */
#define TOS_RECORD_TYPE_UPGRADE_TXN  0x03  /* 升级事务 PENDING→DONE/FAILED */
#define TOS_RECORD_TYPE_COMMAND      0x04  /* 分区间命令（原 REC 尾部命令区） */
#define TOS_RECORD_TYPE_POST_ACTION  0x05  /* 升级完成后的后续目标 */

/* 64 字节/条，环形追加写入 */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t magic;              /* TOS_TEE_STATE_MAGIC */
    uint32_t version;            /* TOS_TEE_STATE_VERSION */
    uint32_t sequence;           /* 全局递增序号 */
    uint32_t type;               /* ↑ 记录类型（新增字段） */

    /* 负载 52 字节，不同类型不同释义 */
    union {
        /* BOOT_TARGET / POST_ACTION */
        struct {
            uint32_t boot_target;     /* RECOVERY / FASTBOOT / NONE */
            uint32_t reserved[12];
        } boot;

        /* LOCK_STATE */
        struct {
            uint32_t unlocked;        /* 0=锁定, 1=解锁 */
            uint32_t reserved[12];
        } lock;

        /* UPGRADE_TXN */
        struct {
            uint32_t txn_state;       /* PENDING / DONE / FAILED */
            uint32_t update_kind;     /* SBL / REC / TEE */
            uint32_t source_address;
            uint32_t target_address;
            uint32_t image_size;
            uint32_t image_crc32;
            uint32_t post_boot_target;
            uint32_t reserved[6];
        } upgrade;

        /* COMMAND（原 REC 命令区内容） */
        struct {
            uint32_t command;         /* FORMAT / UPGRADE / INIT */
            uint32_t arg0;
            uint32_t arg1;
            uint32_t reserved[10];
        } cmd;

        uint8_t raw[52];
    } data;

    uint32_t record_crc;
} TosTeeStateRecord;
```

**原有 `TosTeeStateRecord`（v1）向后兼容**：v1 记录没有 `type` 字段，可默认视为 `UPGRADE_TXN` 类型。新的读取代码遇到 v1 格式时做兼容处理。

### 5.5 状态记录完整性保护

**不需要加密，但需要防篡改完整性保护。**

状态记录区中的数据泄漏出去没有实质危害（锁状态、启动目标本就是公开信息），但被篡改的后果严重——例如 `LOCK_STATE unlocked=0` 被改为 `1` 会直接绕过签名验证。

#### 攻击面分析

| 攻击手段 | 可行性 | 现有防御不足 |
|---------|--------|------------|
| 直接改 `unlocked` 位（Flash 0→1） | ❌ 不可行 | Flash 只能 1→0 写，0→1 必须擦除整扇区 |
| 注入假记录（带正确 CRC） | ✅ 可行 | CRC32 无密钥，已知固定常数可碰撞 |
| 整体替换 TEE 扇区 | ✅ 需要物理拆机 | 无已部署设备时暂不防御此场景 |

#### 方案：CRC32 + 秘密盐值

最经济的升级：在现有 `record_crc` 计算中引入一个**4 字节的秘密盐值**，嵌入 ELF 的 `.rodata` 中。攻击者不知道盐值就无法伪造有效的 `record_crc`。

```c
// 现有（可伪造）:
static inline uint32_t TosTeeStateRecordCrc(const TosTeeStateRecord *r) {
    return r->magic ^ r->version ^ r->sequence ^ r->unlocked ^
           r->boot_target ^ r->update_kind ^ r->source_address ^
           r->target_address ^ r->image_size ^ r->image_crc32 ^
           r->post_boot_target ^ r->reserved0 ^ r->reserved1 ^ r->reserved2 ^
           0xA5C35A3CUL;  // 公开常数
}

// 改进（加秘密盐值）:
// 盐值存储在 ELF .rodata，构建时随机生成
#define TOS_TEE_CRC_SALT  ELF_SECRETS.tee_crc_salt  // 4字节，来自 keys.c

static inline uint32_t TosTeeStateRecordCrc(const TosTeeStateRecord *r) {
    uint32_t crc = 0xFFFFFFFFUL;
    const uint32_t *w = (const uint32_t *)r;
    uint32_t n = (sizeof(TosTeeStateRecord) - 4) / sizeof(uint32_t); // 不含 record_crc
    for (uint32_t i = 0; i < n; ++i) {
        crc ^= w[i];
        for (uint32_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
    return ~crc ^ TOS_TEE_CRC_SALT;
}
```

| 方案 | 成本 | 防伪造 | 兼容性 |
|------|------|--------|--------|
| CRC32 + 固定常数（现状） | 0 | ❌ 可碰撞 | — |
| **CRC32 + 秘密盐值** | 4B 在 ELF，代码几乎不变 | ✅ 需要知道盐值 | ❌ 不兼容旧记录（加盐后 hash 不同） |
| HMAC-SHA256 | +32B 每记录 + 大量代码 | ✅ 密码级 | 过度设计，厂测阶段不需要 |

> **关于兼容性**：引入盐值后旧固件写入的状态记录在新 ELF 下会被判定为无效。对策：
> - **工厂测试阶段（现在）**：直接改，无已部署设备需兼容
> - **量产阶段如需原地固件升级**：ELF 可尝试无盐验证一次再加盐验证，双模式过渡一个版本后弃用旧模式

### 5.6 状态记录区生命周期

```
每条记录 64 字节，后 4KB = 4096 / 64 = 64 条环形日志。

典型升级周期产生的记录数：
  ① SBL 写 UPGRADE_TXN(PENDING)       → +1 条
  ② ELF 改 txn_state DONE/FAILED      → 抹零，不加新条
  ③ REC 写 COMMAND(FORMAT)            → +1 条
  ④ SBL 写 POST_ACTION                → +1 条
  ⑤ 锁定切换 (LOCK_STATE)             → +1 条

→ 一次完整升级 = 2-4 条新记录
→ 64 条满后从最早记录开始覆盖（环形缓冲）
→ 擦除扇区 = 64 条 / (2-4 条每周期) = 16-32 次升级后才需擦一次
→ Flash 擦除寿命 10K 次 → 支持 >16 万次升级操作
```

**寿命结论**：绰绰有余，不需要任何磨损均衡。

---

## 6. TEE 升级方案（方案 A）

### 6.1 问题

TEE 扇区擦除粒度 = 16KB。前 12KB Manifest 需要偶尔升级（公钥轮转、版本更新），但后 4KB 状态记录区运行时频繁写入。不能简单说 TEE "只读"。

### 6.2 方案：ELF 作为 TEE 的升级执行者

ELF 是所有分区（SBL、REC、TEE）的升级执行者。TEE 的 "只读" 约束**只适用于 SBL 和 REC**，不适用于 ELF。

```
TEE 升级流程（与现有 SBL/REC staged 升级完全一致）：

1. HOST (PC 签名工具):
    构建 tee.bin（前 12KB Manifest + TosImageHeader）
       ↓ 签名 (ECDSA P-256)
       ↓ 加密 (AES-128-CBC，同 TMP)
   输出: tee_signed_encrypted.bin

2. FASTBOOT → SBL:
    接收加密镜像
    AES 解密后写入 TMP（明文暂存）
    在 TEE 状态区写: type=UPGRADE_TXN, update_kind=TEE, txn_state=PENDING
    复位

3. ELF 启动:
    读 TEE 状态区 → 发现 TEE_UPDATE_PENDING
    从 TMP 读取 TosImageHeader
    CRC32 快速检查
    SHA-256 摘要验证
    ECDSA 签名验证（用内嵌根公钥）
    版本号检查
    ↓ 全部通过

4. ELF 执行升级:
    ① 将 TEE 后 4KB 状态记录读入 SRAM（保存现场）
    ② 擦除 TEE 整扇区（扇区 3，16KB → 全 0xFF）
    ③ 从 TMP 复制前 12KB 新 Manifest 到 TEE
    ④ 从 SRAM 恢复状态记录到后 4KB

    ⚠️ 步骤 ①→④ 之间掉电：
       如果①已完成但②未完成 → 状态记录在 SRAM 中，但扇区已擦
       → ELF 重试时会发现 TEE 全 0xFF、无有效 magic
       → ELF 再次从 TMP 读取镜像（TMP 在掉电中保留，因擦的是扇区 3）
       → 重试步骤 ②-④，幂等安全

    ⑤ 标记 DONE（抹零 txn_state → 不加新条）
    ⑥ 复位
```

### 6.3 安全保证

| 环节 | 保证 |
|------|------|
| **擦除前** | ECDSA 签名验证通过后才允许擦——未签名的 TPM 内容不会破坏 TEE |
| **掉电** | TMP 保留原镜像（镜像未清），重试即可；状态记录在 SRAM 丢失的窗口极短，且每次启动时 ELF 会检查 TEE 完整性 |
| **状态区保全** | 先读入 SRAM 再擦写，不留数据缝隙 |
| **访问限制** | SBL 和 REC 对 TEE 只有只读权限——他们不能发起擦写，只有 ELF 能在受控升级事务中操作 |

### 6.4 访问控制矩阵修正对照

| 维度 | 旧描述 | 新描述 |
|------|--------|--------|
| SBL 对 TEE | 只读 | 只读 ✅ 不变 |
| REC 对 TEE | 只读 | 只读 ✅ 不变 |
| ELF 对 TEE | 未定义 | **可擦写（仅升级事务）** |
| 状态区写入 | 仅限 TEE 自己？ | **ELF、SBL、REC、SYSTEM 均可以 append**（各有各的写入场景） |

---

## 7. REC 命令迁移

### 7.1 现状

```c
// 当前：REC 命令存在 REC 分区末尾的 1KB
#define TOS_REC_COMMAND_SIZE    0x00000400UL
#define TOS_REC_COMMAND_ADDRESS (TOS_PART_REC_ADDRESS + TOS_PART_REC_SIZE - TOS_REC_COMMAND_SIZE)
```

这个命令区用于 SYSTEM→REC 或 SBL→REC 传递恢复指令（格式化、升级、初始化）。

### 7.2 迁移方案：写入 TEE 状态记录

```
旧的写入方式:
  SYSTEM 直接写 REC 末尾 Flash
  REC 启动时在尾部读命令
  → 不统一、REC 尾部不可签名保护

新的写入方式:
  SYSTEM: 写 TEE 状态区 type=COMMAND, command=FORMAT
  SBL:    写 TEE 状态区 type=COMMAND, command=UPGRADE
  REC:    从 TEE 状态区读最新的 type=COMMAND 记录
  → 统一在 TEE、受环形日志保护、掉电安全
```

**`TOS_REC_COMMAND_ADDRESS` 废弃**，REC 分区尾部的 1KB 空间改为留空或给 REC 固件自身扩展用。

---

## 8. TMP + USERDATA 加密方案

### 8.1 加密策略

| 分区 | 加密算法 | 密钥来源 | 解密时机 | 解密执行者 |
|------|---------|---------|---------|----------|
| **TMP** | AES-128-CBC | ELF 内嵌 16 字节密钥 | SBL 写入 TMP 时解密 | SBL |
| **USERDATA** | AES-128-CBC | ELF 内嵌 16 字节密钥 | SYSTEM 读写时解密 | SYSTEM |

### 8.2 TMP 加密升级流程

```
Host (PC 签名工具)                    设备端
┌──────────────────┐
│ 1. 构建 .bin      │
│ 2. AES-CBC 加密   │
│ 3. 写 TosImageHdr │
│ 4. SHA-256 + ECDSA│
└────────┬─────────┘
         │ FASTBOOT CDC
         ▼
┌──────────────────┐     ┌─────────────────────┐
│ SBL 接收加密数据   │────→│ SBL AES-CBC 解密     │
│ (写入 TMP 前      │     │ → 明文暂存内存       │
│  逐块解密)         │     │ → 明文写入 TMP       │
└──────────────────┘     └──────────┬──────────┘
                                    │ (TMP 暂存明文)
                                    ▼
┌──────────────────────────────────────────────┐
│ ELF (下次启动):                               │
│ 1. 读 TEE 状态记录 → 发现 UPGRADE_TXN(PENDING)│
│ 2. 从 TMP 读 TosImageHeader                    │
│ 3. CRC32 → SHA-256 → ECDSA → 版本号           │
│ 4. 通过 → 编程到 SBL/REC/TEE 目标扇区          │
│ 5. 标记 DONE → 复位                           │
└──────────────────────────────────────────────┘
```

### 8.3 密钥存储

```c
/* partitions/ELF/keys.c */
SBL_CONST const uint8_t tos_root_public_key[64] = { /* 构建时生成 */ };
SBL_CONST const uint8_t tos_aes_key[16] = { /* 构建时生成 */ };
```

**可选增强**：使用 STM32F4 的 96-bit Unique Device ID 派生设备绑定密钥：

```
device_key = SHA-256(UID_96bit || factory_salt_32bit)[0:16]
```

---

## 9. OEM Lock 策略

| 锁定状态 | 签名要求 | FASTBOOT flash | FASTBOOT erase |
|---------|---------|---------------|---------------|
| **Locked**（出厂默认） | ✅ 强制验证 | 拒绝未签名镜像 | 拒绝擦除签名分区 |
| **Unlocked**（开发模式） | ❌ 跳过 | 允许任意镜像 | 允许任意操作 |

锁定状态存储在 TEE LOCK_STATE 记录中。`OEM UNLOCK` 需要物理按键确认。

锁定态下 FASTBOOT 增强：

```
SBL_FlashFinalize() 中:
  ┌─ CRC32 验证（现有）
  │
  ├─ 如果 device is LOCKED && (分区 ∈ {SBL, REC, SYSTEM}):
  │   1. 读 TosImageHeader @ 写地址偏移 0x200
  │   2. 验证 magic + SHA-256 + ECDSA 签名
  │   3. 失败 → 回滚（擦除已写内容），返回 FAIL
  │
  └─ 通过 → 继续 staged/direct 逻辑
```

---

## 10. 签名工具（PC 端）

### 10.1 一次性密钥生成

```bash
# 1. 生成 ECDSA P-256 私钥
openssl ecparam -name prime256v1 -genkey -noout -out tos_privkey.pem

# 2. 导出公钥（64 字节未压缩）
openssl ec -in tos_privkey.pem -pubout -outform DER | \
    tail -c 64 > tos_pubkey_64.bin

# 3. 计算公钥 SHA-256（写入 TEE manifest）
openssl dgst -sha256 tos_pubkey_64.bin

# 4. 生成 AES-128 密钥
openssl rand -hex 16 > tos_aes_key.hex
```

### 10.2 镜像签名流程

```
sign_image.py:
  输入: <partition>.bin + tos_privkey.pem + version
  步骤:
    1. 预留 TosImageHeader 空间（128B 全 FF）
    2. 计算镜像 SHA-256
    3. ECDSA P-256 签名（原始 r||s 格式）
    4. 填充 header（magic, size, CRC32, 摘要, 签名, 版本）
    5. 如果分区 ∈ {TMP}: AES-128-CBC 加密整个 .bin
    6. 输出: <partition>_signed.bin
```

### 10.3 集成到构建流水线

```
构建产 .elf → objcopy → .bin → sign_image.py → signed.bin
                                    │
                             同时生成 pubkey.c + aes_key.c
                             用于编译 ELF 分区
```

---

## 11. 通信总览：谁读写 TEE 状态区

| 写入者 | 记录类型 | 触发场景 | 读取者 |
|--------|---------|---------|--------|
| **SBL** | UPGRADE_TXN(PENDING) | FASTBOOT 写入 TMP 完成后 | ELF |
| **SBL** | POST_ACTION | 多阶段升级需要后续动作 | ELF |
| **SBL** | LOCK_STATE | OEM LOCK/UNLOCK | SBL 自身 |
| **SBL** | BOOT_TARGET | reboot recovery/fastboot | SBL 自身 |
| **ELF** | UPGRADE_TXN(DONE/FAILED) | TMP→目标复制完成后（抹零） | — |
| **SYSTEM** | COMMAND | 请求 REC 格式化/升级 | REC |
| **REC** | BOOT_TARGET | 恢复完成后设置下次启动目标 | SBL |

**所有分区间的数据交换都经过 TEE 状态记录区，没有其他跨分区通信路径。**

---

## 12. 实现路线图

| 阶段 | 内容 | 涉及分区 | 依赖 |
|------|------|---------|------|
| **P0** | 分区布局调整（扇区重排） | 所有分区 | 重新编译全部分区 |
| **P0** | `TosImageHeader` + 增强 `TosTeeManifest` + 统一 `TosTeeStateRecord` | common/include | 无 |
| **P0** | ELF 内嵌公钥 + SHA-256 | ELF | P0 布局 |
| **P0** | ELF 验证 SBL 签名（ECDSA） | ELF + SBL | 签名工具 |
| **P0** | ELF 支持 TEE 升级（保存/恢复状态区） | ELF | P0 + 状态记录定义 |
| **P0** | PC 签名工具 (`sign_image.py`) | PC 工具 | 无 |
| **P1** | SBL 验证 SYSTEM/REC 签名 | SBL | P0 ELF |
| **P1** | REC 命令从 REC 尾部迁移到 TEE 状态区 | REC + TEE | P0 状态记录定义 |
| **P1** | TEE Manifest 版本字段 + 防回滚 | TEE + SBL | P0 |
| **P2** | FASTBOOT 锁定态签名验证 | SBL (flash.c/usb.c) | P1 |
| **P2** | TMP AES 加密（SBL 解密） | SBL + TMP | P1 |
| **P2** | USERDATA AES 加密 | SYSTEM | P2 |
| **P3** | 完整集成测试 | 全系统 | 全部 |

**P0 = 核心安全启动必须**，P1 = 重要增强，P2 = 加密扩展，P3 = 验证收尾。

---

## 13. 与现有系统的兼容性

| 方面 | 状态 |
|------|------|
| **分区布局** | ❌ 已变（扇区 6-11 重排）— 无已部署设备，没问题 |
| **ELF 不可变性** | ✅ 不变，仍是扇区 0 工厂烧录，只是增加了 TEE 升级能力 |
| **TEE 状态记录格式** | ✅ 兼容——`type` 字段放在原 `version` 之后（原 reserved 空间），旧记录 type=0 默认视为 UPGRADE_TXN |
| **REC 命令区** | ❌ `TOS_REC_COMMAND_ADDRESS` 废弃，改用 TEE 状态记录 `type=COMMAND` |
| **SBL 启动流程** | ✅ 不变，仍是向量表检查→验证→跳转 |
| **TMP 暂存更新机制** | ✅ 不变，增加了 TEE 作为第三个可升级的分区 |
| **REC 恢复模式** | ✅ 不变，命令来源从 REC 尾部改为 TEE 状态区 |
| **FASTBOOT USB 协议** | ✅ 不变 |
| **现有 .bin 镜像** | ❌ 需要全部重新编译 + 签名 — 无已部署设备，没问题 |
