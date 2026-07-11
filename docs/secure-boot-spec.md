# TOS Secure Boot 实现与移植规范

本文档描述 slave-board 当前已经实现的安全启动与镜像签名机制，同时作为镜像格式、密钥管理、构建发布、故障处理和平台移植规范。

## 1. 安全目标

当前实现提供：

- ECDSA P-256 镜像来源认证。
- SHA-256 镜像完整性认证。
- 向量表、代码、只读数据、运行时初始化数据和填充区的整体认证。
- 镜像类型、加载地址、签名范围和版本号的签名绑定，防止跨分区替换。
- ELF 在跳转前验证 SBL；SBL 在跳转前验证 REC 和 SYSTEM。
- SBL/REC 升级路径在完成事务前验证目标镜像签名。
- 设备端只包含公钥，私钥不进入设备产物或 dist。

当前不提供固件内容保密。这里的“加密校验”是密码学签名校验，不是镜像加密。以后如需保密，应另行设计 AES-GCM 或 AES-CTR 加 AEAD 封装并使用独立密钥，不能复用 ECDSA 签名密钥。

## 2. 威胁模型

可以防御：

- SD、USB、FASTBOOT 或升级包中的镜像被替换。
- Flash 中 SBL、REC、SYSTEM 的任意单字节篡改。
- 修改 MSP 或 Reset Handler 后跳转到未授权代码。
- 把合法 REC 镜像写入 SYSTEM 等跨分区替换。
- 修改头部地址、类型、大小、版本或摘要。

仅靠本软件不能防御：

- SWD/JTAG 开放时直接改写 ELF 或关闭校验。
- Sector 0 未写保护时替换不可变信任根。
- 电压、时钟、激光故障注入和高级物理攻击。
- 当前没有不可回退硬件计数器，因此不能提供严格持久化防回滚。

量产时必须独立配置 STM32F407 Option Bytes，至少保护 Sector 0，并按产品策略设置 RDP。硬件保护具有高风险，普通构建和烧录脚本不会自动执行。

## 3. 信任链

    STM32 Reset
      -> ELF / Sector 0 / immutable root
           embedded root public key
           verify signed SBL
      -> SBL / Sectors 1-2
           embedded copy of the same root public key
           verify signed SYSTEM or signed REC
      -> SYSTEM or REC

公钥在 ELF 与 SBL 中各保存一份。ELF 内公钥是最终信任锚；ELF 验证整个 SBL，因此 SBL 内公钥副本也被信任链认证。SBL 不调用 ELF 内部函数，SYSTEM/REC 不调用 SBL 内部函数。

TEE Manifest 的 public_key_hash 保存同一公钥的 SHA-256，仅用于一致性诊断，不是信任根。

## 4. 算法和实现

| 项目 | 当前值 |
|---|---|
| 摘要 | SHA-256 |
| 签名 | ECDSA over NIST P-256 / secp256r1 |
| 公钥 | 64 字节 X || Y，各 32 字节大端 |
| 签名 | 64 字节 IEEE P1363 r || s，各 32 字节大端 |
| 头部整数 | 小端 |
| 设备端 ECC | p256-m，提交 44af59e0cff5d3b1d653bc333814077ef830e1bd |
| 许可证 | Apache-2.0 |
| 动态内存 | 不使用 |

SHA-256 位于 partitions/common/secure_boot/sha256.c。P-256 验签来自 p256-m；链接时通过 --gc-sections 删除设备不使用的签名、密钥生成和 ECDH 入口。

## 5. TosImageHeader

头部固定在分区基址加 0x200，大小 128 字节。链接脚本保留 .tos_image_header，并断言向量表不能覆盖该区域。

    typedef struct __attribute__((packed, aligned(4))) {
      uint32_t magic;          /* 0x31474953, SIG1 */
      uint32_t header_version; /* 1 */
      uint32_t image_type;     /* SBL=1, REC=2, SYSTEM=3 */
      uint32_t load_address;
      uint32_t image_size;
      uint32_t image_version;
      uint32_t flags;          /* 当前必须为 0 */
      uint8_t image_digest[32];
      uint8_t signature[64];
      uint32_t header_crc32;
    } TosImageHeader;

| 偏移 | 大小 | 字段 |
|---:|---:|---|
| 0x00 | 4 | magic |
| 0x04 | 4 | header_version |
| 0x08 | 4 | image_type |
| 0x0C | 4 | load_address |
| 0x10 | 4 | image_size |
| 0x14 | 4 | image_version |
| 0x18 | 4 | flags |
| 0x1C | 32 | image_digest |
| 0x3C | 64 | signature |
| 0x7C | 4 | header_crc32 |

header_crc32 使用 IEEE CRC-32，只用于快速发现头部损坏，不承担安全认证。

## 6. 摘要和签名定义

### 6.1 镜像摘要

image_digest 是签名范围内完整镜像的 SHA-256，但计算时把 0x200 到 0x27F 的签名头规范化为 128 字节 0xFF：

    SHA256(
        image[0x000 : 0x200] ||
        FF repeated 128 bytes ||
        image[0x280 : image_size]
    )

这与早期草案只签 0x280 后内容不同。当前定义会认证向量表；否则攻击者可只修改 Reset Handler 绕过安全启动。

| 镜像 | load_address | image_size |
|---|---:|---:|
| SBL | 0x08004000 | 0x8000 |
| REC | 0x08010000 | 0xFC00 |
| SYSTEM | 0x08040000 | 0x80000 |

REC 最后 0x400 字节是运行时命令区，不属于可执行镜像，因此不签名。链接脚本仍保证 REC 代码结束于 0x0801FC00 之前。

### 6.2 签名输入

签名输入为：

    "TOS-SB-P256-V1\0\0"  共 16 字节
    || header[0x00 : 0x1C]
    || image_digest

对以上 76 字节计算 SHA-256，再执行 ECDSA P-256 Sign。这样镜像类型、地址、范围、版本和 flags 都被签名绑定。

## 7. 设备启动验证

### 7.1 ELF 验证 SBL

每次跳转 SBL 前：

1. 检查 MSP、Thumb Reset Handler 和向量地址范围。
2. 检查 magic、格式版本、类型、加载地址、大小和 flags。
3. 检查头部 CRC32。
4. 检查 image_version 大于等于 1。
5. 计算完整 SBL 的 SHA-256。
6. 使用 ELF 内嵌根公钥验证 ECDSA。
7. 全部通过后才清理中断状态并跳转。

任一步失败，ELF 停止启动。

ELF 应用 TMP 中的 SBL/REC 事务时，在擦除目标前验证 TMP 签名，复制完成后再验证目标分区。无效签名不会擦除现有 SBL/REC。

### 7.2 SBL 验证 SYSTEM 和 REC

SBL_AppLooksValid 与 SBL_RecLooksValid 同时要求向量表和密码学签名有效。

SYSTEM 无效时保持 SYSTEM DAMAGE 行为。主动请求 REC 而 REC 无效时进入 RECOVERY EXCEPTION。FASTBOOT 命令通道本身不是可跳转镜像，不执行镜像验签。

### 7.3 升级写入

SBL_FlashFinalize 对 SBL、REC、SYSTEM 强制验签，不因 BL unlocked 状态而跳过：

- SBL/REC 在 TMP 验签后才建立 ELF 事务。
- SYSTEM 直接写入完成后验签。
- TEE、SAH 当前不是启动可执行镜像，不使用 TosImageHeader。

SYSTEM 仍是单槽直接写入。无效或断电中断的更新不会被启动，但可能破坏旧 SYSTEM 的可用性。要同时保证真实性和断电恢复，需要第二槽或外部 staging，这在当前分区不变条件下无法完整实现。

## 8. 密钥管理

### 8.1 开发密钥

未配置生产密钥时，CMake 在项目 sign 目录生成或加载本地开发密钥：

    sign/development-signing-key.pk8
    sign/development-signing-key.pk8.pub
    build/Release/generated/secure_boot/tos_secure_boot_key.h
    build/Release/secure-boot/signing-key.path

私钥不会复制到 dist，且 sign 目录中的密钥材料由 .gitignore 排除。删除或替换该密钥后，新公钥与旧 ELF 不兼容，必须完整 factory 烧录。

### 8.2 生产密钥

生产构建示例：

    $env:TOS_SIGNING_KEY = "D:\protected\tos-production-signing-key.pk8"
    cmake -S . -B build\Release -DCMAKE_BUILD_TYPE=Release
    cmake --build build\Release --target factory_images -- -j24

要求：

- 使用 P-256 PKCS#8 private blob；当前 Windows 工具使用 CNG 格式。
- 私钥不能提交到 Git。
- 量产 CI 应接入 HSM/KMS；保持本文第 5、6 节字节格式不变。
- 更换根密钥必须重新生成并工厂烧录 ELF。
- 禁止把开发密钥用于发布。

## 9. 构建和产物

标准命令：

    cmake --build build\Release --target factory_images -- -j24

流程：

1. 准备或加载根密钥并生成公钥头。
2. 编译所有分区。
3. objcopy 生成完整分区 BIN，空白填充 0xFF。
4. sign_image.ps1 写入 SBL、REC、SYSTEM 签名头。
5. verify_image.ps1 可用公钥独立复验。
6. objcopy --update-section 把同一头部回填到 dist/flash ELF。

安全烧录必须使用：

    dist/flash/factory.elf
    dist/flash/sbl.elf
    dist/flash/rec.elf
    dist/flash/system.elf
    dist/firmware/sbl.bin
    dist/firmware/rec.bin
    dist/firmware/system.bin

build/Release 下的分区 ELF 是未签名链接中间文件，不能直接发布。从旧无签名布局迁移时必须完整 factory 烧录，只刷 SYSTEM 会被新的信任链拒绝。

## 10. 主机验证

SYSTEM 示例：

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts\verify_image.ps1
      -ImagePath dist\firmware\system.bin
      -PublicKeyPath build\Release\secure-boot\development-signing-key.pk8.pub
      -ExpectedType 3
      -ExpectedAddress 0x08040000
      -ExpectedSize 0x80000
      -MinimumVersion 1

SBL 参数为 Type 1、Address 0x08004000、Size 0x8000。

REC 参数为 Type 2、Address 0x08010000、Size 0xFC00。

## 11. 防回滚现状

镜像头含 image_version，构建变量是 TOS_IMAGE_VERSION。设备当前最低版本固定为 1。这能拒绝版本 0，但不是严格防回滚。

严格防回滚要求攻击者无法恢复旧值的单调存储。可擦写 TEE 能被旧快照回放，因此：

- 当前不得宣称已经提供完整防回滚。
- 后续可使用 OTP、Option Bytes 或外部安全元件保存最低版本。
- 若使用 TEE，必须同时解决 TEE 认证、原子更新和快照回放。

## 12. 平台移植步骤

1. 确定不可变首级加载器和硬件写保护。
2. 在链接脚本保留 partition_base + 0x200 的 128 字节头部。
3. 断言向量表不超过 0x200。
4. 保持 TosImageHeader 偏移和小端格式。
5. 移植流式 SHA-256 与 P-256 verify。
6. 调整 Flash 地址类型和缓存同步。
7. 每次跳转前同时检查向量表和签名。
8. 每次擦目标前先验证 staging 镜像。
9. 用 CI/HSM 替换私钥后端，保持签名输入格式。
10. 把硬件保护放在独立量产步骤。
11. 执行测试矩阵并记录公钥指纹、版本和批次。

若新平台向量表超过 0x200，必须整体调整 HeaderOffset，并同步链接器、签名器和验证器。

## 13. 测试矩阵

| 测试 | 预期 |
|---|---|
| 正常签名 SBL | ELF 跳转 SBL |
| SBL 代码或 Reset Handler 翻转 | ELF 停止 |
| SBL 类型改为 REC | ELF 拒绝 |
| 正常签名 SYSTEM | 启动 SYSTEM |
| SYSTEM 任意字节翻转 | SYSTEM DAMAGE |
| SYSTEM 地址或大小改动 | SBL 拒绝 |
| 正常签名 REC | 主动 REC 正常 |
| REC 签名范围字节翻转 | RECOVERY EXCEPTION |
| REC 最后 1KB 命令区变化 | 不影响 REC 镜像签名 |
| TMP 中无效 SBL | ELF 不擦目标 SBL |
| 旧无签名镜像 | 拒绝 |
| 公钥不匹配 | 拒绝 |
| 主机单字节篡改 | verify_image.ps1 失败 |

## 14. 当前本机构建结果

| 目标 | Flash 使用 | 上限 |
|---|---:|---:|
| ELF | 5,508 B | 16 KB |
| SBL | 27,360 B | 32 KB |
| REC | 52,532 B | 63 KB |
| SYSTEM | 223,736 B | 512 KB |

签名头地址：

    SBL     0x08004200
    REC     0x08010200
    SYSTEM  0x08040200

三个向量表大小均为 0x188，未覆盖签名头。SBL、REC、SYSTEM 公钥复验通过。SYSTEM 在偏移 0x300 翻转一个字节后被摘要检查拒绝。

## 15. 相关文件

    partitions/common/include/tee_format.h
    partitions/common/include/tos_secure_boot.h
    partitions/common/include/tos_sha256.h
    partitions/common/secure_boot/image_header.c
    partitions/common/secure_boot/sha256.c
    partitions/common/secure_boot/verify.c
    partitions/common/secure_boot/third_party/p256-m/
    scripts/prepare_secure_boot_key.ps1
    scripts/sign_image.ps1
    scripts/verify_image.ps1
    scripts/export_partitions.ps1
    ld/sbl.ld
    ld/rec.ld
    ld/system.ld

## 16. 参考实现与依据

- STMicroelectronics, Introduction to Secure Boot and Secure Firmware Update:
  https://wiki.st.com/stm32mcu/wiki/Security:Introduction_to_Secure_boot_and_Secure_firmware_update
- STMicroelectronics, X-CUBE-SBSFU:
  https://www.st.com/en/embedded-software/x-cube-sbsfu.html
- STMicroelectronics AN5056, X-CUBE-SBSFU integration guide:
  https://www.st.com/resource/en/application_note/an5056-integration-guide-for-the-xcubesbsfu-stm32cube-expansion-package-stmicroelectronics.pdf
- MCUboot signed image design:
  https://docs.mcuboot.com/signed_images.html
- p256-m upstream:
  https://github.com/mpg/p256-m

当前 p256-m 版本体积适合 16KB ELF，但上游明确说明尚未经过独立安全审计。量产安全等级要求较高时，应安排代码审计，或评估 X-CUBE-CRYPTOLIB、经认证的供应商密码库；替换设备端实现时必须保持本文定义的公钥、签名和签名输入格式。
