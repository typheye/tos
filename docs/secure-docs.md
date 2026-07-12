# 安全策略

> 算法: ECDSA P-256 + SHA-256 | 信任根: ELF (扇区 0 不可变)

---

## 1. 信任链

```
ELF (内嵌公钥, 不可变)
  │ ECDSA P-256 + SHA-256
  ▼
SBL
  │ ECDSA P-256 + SHA-256
  ▼
SYSTEM / REC
```

## 2. 签名头

每个签名分区 (SBL / REC / SYSTEM) 偏移 `0x200` 处有 128 字节 `TosImageHeader`:

| 偏移 | 大小 | 字段 |
|------|------|------|
| 0 | 4 | magic `0x31474953` |
| 8 | 4 | image_type (1=SBL, 2=REC, 3=SYSTEM) |
| 12 | 4 | load_address |
| 16 | 4 | image_size |
| 28 | 32 | SHA-256 digest (header 占位 0xFF) |
| 60 | 64 | ECDSA P-256 signature (r||s) |
| 124 | 4 | header_crc32 |

## 3. 构建签名

```bash
cmake --build build/Release --target factory_images
```

`sign_image.ps1` 自动:
1. objcopy ELF → BIN (`--gap-fill 0xFF --pad-to`)
2. SHA-256 全镜像 (header 区替换为 0xFF)
3. ECDSA P-256 签名 (原始 r||s)
4. 写入 TosImageHeader
5. `verify_image.ps1` 自检

## 4. OEM Lock / Unlock

| 状态 | SBL 验证 SYSTEM | FASTBOOT flash 签名 |
|------|----------------|-------------------|
| Locked | ECDSA 强制 | 写入后当场验签, 失败回滚 |
| Unlocked | 仅向量表 | 跳过验签 |

ELF 验证 SBL 永远强制, 不受解锁状态影响。

## 5. 重启加速

| 场景 | 标志 | 耗时 |
|------|------|------|
| 冷启动 (上电) | 无 | ~2s |
| 软复位 | RESTART(NONE) | ~0.5s |
| 定向重启 | RESTART(target) | ~0.5s |

## 6. SYSTEM DAMAGE 页面

验证失败时 LCD 显示具体原因:

| 错误码 | 显示 |
|--------|------|
| BAD_HEADER | System image invalid. |
| BAD_CRC | System image corrupted. |
| BAD_DIGEST | System image mismatch. |
| BAD_SIGNATURE | System signature invalid. |
| ROLLBACK | System version too old. |
