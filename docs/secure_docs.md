# TOS 安全启动改造思路与开发记录

本文档记录本轮安全启动工作的分析过程、关键决策、实现步骤、问题修正和后续维护原则。正式的字节级协议与移植规范见 docs/secure-boot-spec.md。

## 1. 任务起点

原始目标是在不改变 STM32F407ZGT6 现有分区布局的前提下，实现：

- SBL、REC、SYSTEM 启动镜像完整性校验。
- P-256 镜像签名。
- ELF 到 SBL，再到 SYSTEM/REC 的逐级信任链。
- 构建后自动签名。
- 为后续平台移植留下稳定规范。

现有布局中最重要的约束是：

| 分区 | 地址 | 大小 |
|---|---:|---:|
| ELF | 0x08000000 | 16KB |
| SBL | 0x08004000 | 32KB |
| TEE | 0x0800C000 | 16KB |
| REC | 0x08010000 | 64KB |
| SYSTEM | 0x08040000 | 512KB |

ELF 是最先执行且计划写保护的区域，因此只能把 ELF 作为最终信任根。TEE 是可写数据区，不能独立承担根公钥信任。

## 2. 对原始草案的审查

原始 secure-boot-spec.md 给出的总体方向正确：

- ECDSA P-256。
- SHA-256。
- ELF 验证 SBL。
- SBL 验证 SYSTEM 和 REC。
- 构建端生成签名头。

但草案有几个不能直接照搬的问题。

### 2.1 向量表没有被认证

草案建议只对 0x280 之后的数据计算摘要。这样 0x000 到 0x1FF 的向量表没有签名，攻击者可以只修改 Reset Handler，让 CPU 跳转到其他 Flash 代码。

最终方案改为认证完整镜像，只在计算摘要时把签名头本身规范化成 0xFF。这样既避免签名自引用，又覆盖 MSP、Reset Handler 和整个向量表。

### 2.2 签名只覆盖摘要还不够

如果签名只绑定镜像摘要，同一份合法镜像可能被复制到另一个分区。

最终签名输入额外绑定：

- image_type。
- load_address。
- image_size。
- image_version。
- flags。
- image_digest。

因此合法 REC 不能伪装成 SYSTEM，合法 SYSTEM 也不能被移动到其他地址执行。

### 2.3 TEE 不能作为根公钥来源

TEE 分区允许更新，也包含运行状态。如果 SBL 直接信任 TEE 中的公钥，攻击者只要同时替换 TEE 和目标镜像，就可能建立自己的信任链。

最终设计是：

- ELF 内嵌根公钥。
- SBL 内嵌同一公钥副本。
- ELF 验证整个 SBL，所以 SBL 内公钥副本受到 ELF 保护。
- TEE 只保存 public_key_hash 用于一致性诊断。

### 2.4 防回滚不能只靠可写 TEE

把 minimum version 放进可擦写 TEE，并不能阻止攻击者回放旧的 TEE 快照。

本轮保留 image_version 字段，但明确不宣称已经提供严格防回滚。真正的最低版本需要 OTP、Option Bytes、外部安全元件或其他不可回退存储。

## 3. 密码库选择

最初考虑过 mbedTLS 和 micro-ecc，但 ELF 只有 16KB，SBL 只有 32KB，完整 mbedTLS ECC 组件体积风险很高。

最终选择 p256-m，原因是：

- 专门面向 32 位受限 MCU。
- Cortex-M4 代码约 2.9KB。
- 无堆分配。
- 提供标准 P-256 ECDSA verify。
- 输入格式正好是 64 字节公钥 X || Y 和 64 字节签名 r || s。
- Apache-2.0 许可证。

引入版本固定为：

    44af59e0cff5d3b1d653bc333814077ef830e1bd

代码放在：

    partitions/common/secure_boot/third_party/p256-m/

上游说明该实现尚未经过独立安全审计，因此量产前应安排审计，或者评估经认证的密码库。固定提交的目的，是避免后续构建因上游变化产生不可追踪差异。

## 4. 第一轮：定义稳定镜像格式

新增 TosImageHeader，固定在每个可执行分区基址加 0x200，大小 128 字节。

选择 0x200 的原因：

- 当前 STM32F407 向量表大小为 0x188。
- 0x200 对齐清晰。
- 与原始草案兼容。
- 头部之后仍可继续放置代码。

修改了 SBL、REC、SYSTEM 链接脚本：

- 显式保留 .tos_image_header。
- 断言向量表不超过 0x200。
- 断言头部必须正好 0x80 字节。
- 链接器自动把后续代码放到头部之后。

没有修改 CubeMX 自动生成文件。

## 5. 第二轮：设备端摘要和签名验证

实现了流式 SHA-256：

    partitions/common/secure_boot/sha256.c

实现统一验证入口：

    TosSecureBootVerify(
        storage_address,
        expected_load_address,
        expected_size,
        expected_type,
        minimum_version)

统一验证器负责：

1. 检查参数和地址溢出。
2. 检查 magic、header_version、type、address、size、flags。
3. 检查头部 CRC32。
4. 检查最低版本。
5. 重新计算完整镜像 SHA-256。
6. 常量时间比较摘要。
7. 对规范化签名输入计算 SHA-256。
8. 使用 P-256 根公钥验证 r || s。

统一入口避免 ELF、SBL、REC 分别维护不同的验证规则。

## 6. 第三轮：接入启动链

### 6.1 ELF

ELF 的职责增加为：

- 正常启动前验证目标 SBL。
- TMP 中存在 SBL/REC 事务时，先验证 staging 镜像。
- 验证通过后才擦除目标分区。
- 写入完成后再次验证目标分区。
- 验证失败时不执行未经授权代码。

这里保留了原有 CRC32 事务检查。CRC 用于快速发现传输损坏，ECDSA 用于来源认证，两者职责不同。

### 6.2 SBL

SBL_AppLooksValid 和 SBL_RecLooksValid 从单纯向量表检查升级为：

    vector valid AND signature valid

保留现有故障界面语义：

- SYSTEM 无效进入 SYSTEM DAMAGE。
- 主动进入 REC 但 REC 无效时进入 RECOVERY EXCEPTION。
- FASTBOOT 仍是命令通道，不作为启动镜像验签。

### 6.3 升级 finalize

SBL_FlashFinalize 对 SBL、REC、SYSTEM 强制验签：

- SBL 与 REC 在 TMP 中验证。
- SYSTEM 在直接写入后验证。
- BL unlocked 不会绕过镜像签名检查。

REC 使用了同一套 flash.c，因此 REC 目标也链接了统一验证器，避免 REC 升级与 FASTBOOT 升级规则不一致。

## 7. 第四轮：主机密钥和签名工具

本机没有可直接依赖的 OpenSSL 和 Python cryptography 环境，因此没有强行引入额外运行时，而是使用 Windows 自带 CNG：

- ECDsaCng 生成和加载 P-256 密钥。
- 私钥使用 PKCS#8 private blob。
- 公钥导出为 CNG EccPublicBlob，再提取 X 和 Y。
- SignHash 输出 64 字节 P1363 r || s。

新增工具：

| 文件 | 作用 |
|---|---|
| prepare_secure_boot_key.ps1 | 创建或加载密钥，生成设备公钥头 |
| sign_image.ps1 | 生成摘要、签名并写 TosImageHeader |
| verify_image.ps1 | 只使用公开密钥独立复验镜像 |
| export_partitions.ps1 | 导出 BIN、签名、复验并回填 ELF |

签名脚本会自检刚生成的签名。导出脚本还会调用独立公钥验证器，避免格式错误或密钥不一致的镜像进入 dist。

## 8. 第五轮：签名 BIN 与烧录 ELF 一致

项目工厂烧录使用 dist/flash 下的 ELF，而升级包使用 dist/firmware 下的 BIN。

如果只签 BIN，OpenOCD 工厂烧录 ELF 时仍会写入空白签名头，设备无法启动。

因此导出流程采用：

1. 从 build ELF 导出完整分区 BIN。
2. 在 BIN 中计算摘要并签名。
3. 保存 128 字节头部。
4. 使用 objcopy --update-section 把相同头部写入 dist/flash ELF。
5. 用公开密钥复验 BIN。

这样工厂 ELF 和升级 BIN 使用完全相同的签名元数据。

build/Release 下原始分区 ELF 仍是未签名中间产物；安全发布必须使用 dist/flash。

## 9. 第六轮：构建问题与修复

实现过程中出现并修复了以下问题。

### 9.1 生成公钥 include 顺序

最初 PartitionTargets.cmake 在 TOS_SECURE_BOOT_GENERATED_DIR 定义前被 include，导致公共 include 列表没有生成目录。

修复为：

- 先配置密钥路径。
- 运行 prepare_secure_boot_key.ps1。
- 再 include PartitionTargets.cmake。

### 9.2 REC 共用 flash.c 的链接依赖

flash.c 增加签名验证后，REC 目标也需要 SHA-256、P-256 和 verify.c。

修复为在 cmake/rec/CMakeLists.txt 中加入同一验证组件。

### 9.3 PowerShell 高位 UInt32 常量

PowerShell 5.1 会把某些十六进制高位常量解释成负数，CRC32 的 0xFFFFFFFF 和 0xEDB88320 出现类型转换错误。

修复为使用 UInt32::MaxValue 和无歧义十进制常量。

### 9.4 PowerShell 泛型语法兼容

PowerShell 5.1 无法解析原先的 SequenceEqual 泛型调用。

修复为固定 32 字节异或累积比较，兼容 Windows PowerShell 5.1。

## 10. 第七轮：密钥目录整理

开发密钥最初位于：

    build/Release/secure-boot/

问题是删除构建目录会意外丢失信任根，随后生成的新密钥会让旧设备镜像全部失效。

现已迁移为：

    sign/development-signing-key.pk8
    sign/development-signing-key.pk8.pub

CMake 默认从 sign 目录加载开发密钥。build/Release/secure-boot 只保存：

- signing-key.path。
- 各镜像临时 header.bin。
- 验证测试临时产物。

.gitignore 规则为：

    sign/*
    !sign/README.md

因此版本库会保留目录说明，但不会提交任何私钥或派生公钥文件。

生产构建仍应显式传入外部受保护密钥，不应把生产私钥放在源码目录。

## 11. 最终签名范围

| 镜像 | 类型 | 地址 | 签名范围 |
|---|---:|---:|---:|
| SBL | 1 | 0x08004000 | 0x8000 |
| REC | 2 | 0x08010000 | 0xFC00 |
| SYSTEM | 3 | 0x08040000 | 0x80000 |

REC 最后 0x400 字节为运行时命令区，不在签名范围。该区域不能包含可执行代码或静态安全策略。

TEE 与 SAH 当前没有作为启动可执行镜像签名：

- TEE 是 Manifest 和状态存储。
- SAH 是启动图像数据。

如果以后让 TEE 承担可执行安全服务，必须单独定义 TEE 的认证范围，不能直接复用当前未签名布局。

## 12. 最终构建与验证结果

执行：

    cmake --build build\Release --target factory_images -- -j24

设备端 Flash 使用：

| 目标 | 使用 | 上限 |
|---|---:|---:|
| ELF | 5,508 B | 16KB |
| SBL | 27,360 B | 32KB |
| REC | 52,532 B | 63KB |
| SYSTEM | 223,736 B | 512KB |

导出后的签名头地址：

    SBL     0x08004200
    REC     0x08010200
    SYSTEM  0x08040200

向量表大小均为 0x188，没有覆盖签名头。

验证结果：

- SBL 公钥复验通过。
- REC 公钥复验通过。
- SYSTEM 公钥复验通过。
- SYSTEM 偏移 0x300 单字节翻转后被拒绝。
- dist 中不存在 pk8、pem、private 或 key 文件。
- ELF 和 SBL 都实际链接了 TosSecureBootVerify、root_public_key 和 p256_ecdsa_verify。

## 13. 重要安全结论

### 13.1 当前提供真实性，不提供保密性

攻击者仍可读取 BIN 内容，但不能生成合法的新签名。需要保密时必须另行设计加密封装。

### 13.2 软件安全启动必须配合硬件保护

如果 Sector 0 可以通过 SWD 被改写，攻击者可以替换 ELF 和根公钥。量产必须设置 WRP/RDP，并把操作放在独立工厂流程。

### 13.3 当前版本字段不是严格防回滚

image_version 已经被签名，但最低版本仍固定为 1。没有不可回退存储时，旧的合法签名镜像仍可能被重新安装。

### 13.4 SYSTEM 仍是单槽更新

签名能保证损坏镜像不执行，但不能让被中断的 SYSTEM 更新自动恢复旧版本。完整断电恢复需要第二槽或外部 staging。

## 14. 后续接手原则

后续修改安全启动时必须遵守：

1. 不得取消向量表认证。
2. 不得从可写 TEE 加载根公钥。
3. 不得在 unlocked 模式跳过启动验签。
4. 不得把私钥复制到 dist。
5. 不得直接烧录 build/Release 下的未签名 ELF。
6. 修改头格式必须同步设备验证器、签名器、验证器和链接脚本。
7. 修改 REC 命令区大小时必须同步 TOS_PART_REC_SIGNED_SIZE。
8. 更换密钥后必须完整 factory 烧录 ELF、SBL、REC、SYSTEM。
9. 量产前必须替换开发密钥并审查密码库。
10. 每次发布至少执行正常签名、单字节篡改、错误类型、错误地址和错误公钥测试。

## 15. 文档关系

- docs/secure-boot-spec.md：正式协议、运行规则和移植规范。
- docs/secure_docs.md：本文件，记录设计推导、实现轮次和维护思路。
- sign/README.md：本地密钥目录规则。
- partitions/common/secure_boot/third_party/p256-m/README.tos.md：第三方代码来源与固定提交。
