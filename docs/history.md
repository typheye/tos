# TOS 项目上下文与开发交接

工作区：

```text
C:\Code\tos
```

主要工程：

```text
slave-board  STM32F407 + ESP8266 项目，也是目前主要开发对象
win-pc       SBL/FASTBOOT 上位机工具
```

请先执行 `git status`、检查现有源码和未提交改动。不要仅凭本说明覆盖代码，也不要撤销已有修改。

## 一、最初目标

无需了解，绝大部分工作转向了 `slave-board`。

## 二、必须遵守的开发原则

1. 不要直接修改 CubeMX 会重新生成的文件内容。

尤其不要再直接修改：

```text
startup_stm32f407xx.s
```

自定义启动逻辑放在：

```text
slave-board/SBL/boot.s
```

对于 `Core/Src/*.c`、USB 等 CubeMX 文件，只能尽量修改 `USER CODE BEGIN/END` 区域。

2. SBL、REC、SAH 必须独立于 SYSTEM。

即使 `system` 被擦除，SBL 仍必须能：

- 点亮 LCD。
- 检测 PA15。
- 进入 FASTBOOT。
- 使用 USB CDC。
- 检查 REC。
- 显示 SYSTEM DAMAGE / RECOVERY EXCEPTION。
- 擦写允许操作的分区。

不得让 SBL/REC 的早期代码、常量、`memset/memcpy`、中断默认处理函数或 FatFs 函数意外链接到 SYSTEM。

3. UI 避免整屏清黑和肉眼可见的从上到下绘制。

首屏或模式切换推荐：

```text
关闭显示或背光
-> 在隐藏状态完整绘制
-> 最后一次性显示
```

后续状态变化尽量局部刷新，例如只刷新倒计时数字或状态行。

4. 启动链路不能出现黑屏闪断。

SAH/SBL 阶段亮度固定 100%，只有进入 TOS 且 `PD_SplashFinish()` 完成后，才应用方向、手动亮度或自动亮度设置。

5. 不要为了新增一个设置项不断改变设置 magic 并写专用迁移代码。

设置存储已改为稳定 magic family、结构前缀兼容、缺失字段使用默认值。恢复出厂设置应统一擦除配置并回到完整默认值。

## 三、当前 Flash 分区

当前最终采用的 STM32F407 分区为：

```text
SBL     0x08000000  64KB   不允许通过 FASTBOOT 擦写
REC     0x08010000  64KB   可擦可写
SAH     0x08020000  128KB  可擦可写
SYSTEM  0x08040000  512KB  可擦可写
```

导出表中的相对偏移：

```text
sbl,0x00000000,0x00010000
rec,0x00010000,0x00010000
sah,0x00020000,0x00020000
system,0x00040000,0x00080000
```

FLASH 尾部仍有 TOS 用户设置/数据区域，不能生成或刷入覆盖整个 Flash 的 `full.bin`。

导出目录：

```text
slave-board/dist/partitions.csv
slave-board/dist/firmware/sbl.bin
slave-board/dist/firmware/rec.bin
slave-board/dist/firmware/sah.bin
slave-board/dist/firmware/system.bin
```

导出脚本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\export_partitions.ps1
```

## 四、启动链路

设计上的启动顺序：

```text
STM32 ROM Bootloader
-> SBL/boot.s 自定义 Reset_Handler
-> SBL 最小硬件初始化
-> SBL 绘制启动画面，图像数据来自 SAH
-> 消费一次性启动目标
-> 根据状态进入 REC / FASTBOOT / SYSTEM
```

SAH 当前是 logo 数据分区，不再承担主要逻辑。Splash、解锁小锁、启动判断主要由 SBL 实现。

正常启动时：

- SBL 从 SAH 读取 logo。
- logo 至少完整亮屏约 1.6 秒。
- TOS 接管时保留原 logo。
- TOS 只在底部绘制白灰进度条。
- `PD_SplashFinish()` 将进度平滑补到 100%，停约 500ms，再淡出约 300ms。
- 不能在 SAH -> TOS 之间黑屏。

如果 SAH 数据损坏，系统不应因此完全无法启动；最多没有 logo。解锁小锁仍应能显示。

## 五、SBL 状态区域

SBL 代码被限制在前 48KB。

SBL 最后一个 16KB sector 作为状态 sector，其中逻辑状态区位于：

```text
0x0800FC00
```

用于保存：

- Bootloader lock/unlock 状态。
- 一次性启动目标。
- REC format/upgrade 等命令。

写入失败或无空槽时，可以擦除状态 sector 后重新写入，但必须保证 SBL 正文绝不进入这个 sector。

一次性启动目标读取后，不需要擦整块 1KB，只需把 magic 写成无效值，例如 `0x00000000`。

启动目标优先级高于 PA15：

```text
有效启动目标
-> REC / REC FORMAT / REC UPGRADE / FASTBOOT
否则
-> 检查 PA15
-> 正常 SYSTEM
```

## 六、FASTBOOT / USB CDC

SBL 在 FASTBOOT 下把板载 Type-C 虚拟成 USB CDC 串口。

主要文件：

```text
slave-board/SBL/usb.c
slave-board/SBL/include/sbl_usb.h
slave-board/SBL/ui.c
slave-board/SBL/init.c
slave-board/SBL/state.c
slave-board/SBL/common.c
slave-board/SBL/hw.c
slave-board/SBL/lcd.c
```

实现过的稳定性策略：

- FASTBOOT 画面先显示，再初始化 USB，避免 USB 时钟失败导致卡在第一屏。
- USB 时钟使用 SBL 独立的 HSE/PLL 寄存器配置。
- USB FS 需要稳定的 48MHz PLLQ。
- 初始化前将 PA12/DP 拉低约 80ms，强制主机重新枚举。
- FASTBOOT 循环周期检测 USB，掉线后执行断开脉冲并重新初始化。
- USB 热插拔后不能永久变成未知设备。
- USB 初始化失败不能卡死 PA15 菜单。
- `Reboot to bootloader` 必须真实复位，不是假重载界面。
- 300 秒无操作自动复位。

SBL 菜单最新意图：

```text
Reboot
Reboot to recovery
Reboot to bootloader
```

`oem unlock`：

- 仅未解锁时显示确认 UI。
- PA15 短按切换 YES/NO，长按立即确认。
- NO 只返回 FASTBOOT，不改变状态。
- 已解锁再次执行应直接提示已经解锁，不改变 UI。
- 确认页 30 秒无操作返回。
- 解锁成功后按既有设计进入 REC 做格式化流程。

`oem lock`：

- 仅已解锁时有效。
- 未解锁时提示无需处理。
- 锁定成功后按设计进入 REC 格式化或重启。
- 状态刷新后 FASTBOOT 页显示正确的 `unlocked:no`。

上位机工具：

```text
C:\Code\tos\win-pc\src\sbltools.py
```

常用命令形式：

```powershell
python sbltools.py --port COM8 getvar all
python sbltools.py --port COM8 oem unlock
python sbltools.py --port COM8 oem lock
python sbltools.py --port COM8 flash system firmware/system.bin
python sbltools.py --port COM8 flash sah firmware/sah.bin
python sbltools.py --port COM8 erase system
python sbltools.py --port COM8 reboot
```

只有 unlocked 状态允许 flash/erase。SBL 自身禁止通过 FASTBOOT 更新。

## 七、REC 当前设计

原名 SRE，已经统一改名为 REC：

```text
slave-board/REC
dist/firmware/rec.bin
```

REC 模式包括：

```text
REC_MODE_WAIT
REC_MODE_FORMAT
REC_MODE_UPGRADE
```

REC 首屏：

```text
Recovery Mode

Awaiting instructions...
```

主动进入、没有命令时，60 秒后自动重启。

如果启动目标要求进入 REC，但 REC 分区无效或被擦除，SBL 应显示：

```text
RECOVERY EXCEPTION
Restart the system after 5 s...
```

倒计时只刷新数字，不能整行闪烁。倒计时后自动重启。用户仍应能在下一次开机时通过 PA15 进入 FASTBOOT。

Debug 页已经存在：

```text
03 Enter FASTBOOT
   Enter Recovery
04 Upgrade from REC
```

`04 Upgrade from REC` 会确认后写入 `RUPG` 启动目标并重启进入 REC 卡刷。

卡刷期望 SD 卡文件：

```text
/init
/data/upgrade/partitions.csv
/data/upgrade/firmware/sah.bin
/data/upgrade/firmware/system.bin
```

目前策略：

- 支持升级 SAH 和 SYSTEM。
- 不允许 REC 在运行时擦写自己。
- SBL 保持保护，不通过 REC 自刷。
- 分区镜像需要做大小和完整性校验。
- 升级过程需要显示状态/进度并有明确错误原因。

## 八、REC 已做但仍有问题的部分

第一版用手写 FAT32 解析，后来改为 REC 自带 FatFs。

相关文件：

```text
slave-board/REC/init.c
slave-board/REC/fat.c
slave-board/REC/include/rec.h
slave-board/cmake/rec/CMakeLists.txt
slave-board/STM32F407XX_FLASH.ld
```

已做过：

- REC 内独立包含 SDIO/HAL_SD polling 读卡代码。
- FatFs 核心函数放入 REC 分区，避免调用 SYSTEM 中的 FatFs。
- 挂载路径使用：
  - `0:/init`
  - `0:/storage/tos/upgrade/partitions.csv`
  - `0:/storage/tos/upgrade/firmware/sah.bin`
  - `0:/storage/tos/upgrade/firmware/system.bin`

- 尝试参考 TOS 的稳定流程：
  - `HAL_SD_Init`
  - 延时
  - CardInfo 重试
  - 尝试 4-bit
  - 等待 card ready

- 尝试在以下目录记录 REC 日志：

  ```text
  0:/system/rec/00000001.log
  0:/system/rec/00000002.log
  ```

- 日志格式参考 TOS：

  ```text
  [    1.234] [INFO ] [REC  ] Recovery log started
  ```

上一次成功编译时 REC 约使用：

```text
约 19KB / 64KB
```

但是实体卡测试仍然失败，界面显示：

```text
Mount SD...
Upgrade failed: /init missing
```

用户确认 SD 卡和 `/init` 文件没有问题，因此不能继续把它简单归因于路径不存在。

## 九、当前最重要的未完成任务

最后一轮只进行了代码阅读和原因分析，没有真正完成修改。

当前最高概率根因：

REC 是在 SBL splash 后直接进入的，但完整的：

```text
168MHz 系统时钟
PLLQ 48MHz USB 时钟
```

此前只在 FASTBOOT 分支中配置。

SDIO 与 USB 都依赖完整时钟链，因此 REC 可能是在不完整时钟状态下调用 `HAL_SD_Init/f_mount`，导致表现不稳定，并最终被误报成 `/init missing`。

下一步应优先完成：

1. 在进入 REC 前建立独立、带超时的完整时钟配置。

不能依赖 SYSTEM 的 `SystemClock_Config()`，也不能无限等待 HSE/LSE。

2. 把 TOS 已验证的 SDIO/FatFs 链完整对齐到 REC。

重点比较：

```text
FATFS/Target/sd_diskio.c
FATFS/Target/bsp_driver_sd.c
FATFS/Target/fatfs_platform.c
FATFS/App/fatfs.c
src/hardware/tsdio.cpp
```

检查：

- GPIO Alternate Function。
- SDIO clock。
- DMA/IRQ 是否真的需要。
- polling 模式下 HAL 状态。
- `HAL_SD_DeInit` 后重新初始化。
- 1-bit/4-bit 切换。
- 卡 ready 轮询。
- disk status / initialize / read / ioctl。
- FatFs driver link。
- 路径盘符。
- 堆栈和对齐。
- REC 是否仍引用 SYSTEM 地址。

3. 错误必须分层显示和记录。

不要所有失败都显示 `/init missing`。至少区分：

```text
Clock init failed
SDIO init failed
Card info failed
Card not ready
Disk driver link failed
f_mount failed: <FRESULT>
f_stat /init failed: <FRESULT>
partitions.csv missing
Image open/read failed
Flash erase/write/verify failed
```

SD 卡没挂载成功时无法写 SD 日志，所以应同时把关键错误显示到 LCD；必要时保留 RAM 中诊断码。

4. 让 REC 使用现有统一动态内存。

不要再建第三套 heap。REC 应复用 SBL 中的 `SysDram`/内存接口，并确保 DMA buffer 来自普通 SRAM，不来自 CCMRAM。

5. 在 REC 中增加 USB Mass Storage。

用户最新要求：

- 进入 REC 后，把 Type-C 枚举为 USB MSC。
- MSC 后端直接映射 SD 卡。
- 不要复用 SBL CDC 描述符。
- REC 应有自己的 MSC class、descriptor 和 storage backend。
- 参考 SBL 的免插拔重新枚举策略：
  - D+ 断开脉冲。
  - 周期检测配置状态。
  - 掉线后 DeInit/ReInit。
  - 插拔后仍能重新识别。

- USB 与 REC 卡刷不能同时写 SD。
  - 主机挂载 MSC 时，REC 不得同时让 FatFs 写入。
  - 执行升级前应停用/断开 MSC，卸载 FatFs，再独占 SD。
  - 升级结束后按需要重新枚举。

最后一轮尚未实现 MSC，也尚未提交上述时钟修复。

## 十、动态内存现状

统一动态内存模块已经下放到 SBL，并由 SBL 与 TOS 共用，不是两套 heap。

实现包括：

```text
malloc/free/calloc/realloc
_malloc_r/_free_r/_calloc_r/_realloc_r
SysDram_AllocFast
SysDram_AllocDma
```

策略：

- `SysDram_AllocDma()` 只能使用普通 SRAM，供 LCD、SDIO、USB DMA。
- `SysDram_AllocFast()` 优先使用 CCMRAM。
- 通用 malloc 目前以稳定性优先，不能随便把可能交给 DMA 的库内存放进 CCMRAM。
- 动态池边界从 linker symbol 推导。
- SRAM 池保留约 8KB 栈保护，防止 FatFs/网络/日志深栈踩堆。
- LCD tile buffer 使用动态 DMA-safe 内存。
- 当前 `TILE_HEIGHT` 大约为 60，是 RAM 与刷新速度的折中。
- 不要重新用巨大静态数组把链接器 RAM 人为填到 100%。

之前曾出现：

- `TILE_HEIGHT=80`：约 38.4KB LCD buffer，RAM 高但速度较好。
- `TILE_HEIGHT=16`：RAM 降低，但 FPS 跌到约 1，肉眼可见逐行绘制。
- 最后折中为 60，约四块 tile 刷完整屏。

## 十一、TOS UI 与设置方面已有修改

已增加 `src/library/libui.cpp/.h`，统一封装：

- Frame title。
- 标题时间和状态图标。
- 菜单卡片。
- 布尔值/右侧值。
- Footer。
- 菜单可视区域计算。

页面切换尽量不再先执行全屏黑色清屏。

About 页：

```text
03 Running <h:mm:ss>
```

替代原来的 System Information，并实时显示启动时间。

连续点击 Build 五次：

- 弹出 confirm。
- 标题 `DEBUG`。
- 内容 `Whether to enter debugging settings?`
- Yes 才进入 DEBUG。

DEBUG 页包括：

```text
00 Return
01 Dashboard <ON/OFF>
02 Logd on COM <ON/OFF>
03 Enter FASTBOOT
   Enter Recovery
04 Upgrade from REC
```

Dashboard 调试面板：

```text
FPS:   0F | CPU:   0%
RAM:   0% | CRM:   0%
```

- 紧贴屏幕右下角。
- 绿色显示 FPS/CPU。
- 黄色显示 RAM/CCMRAM。
- `syshandle` 异常页禁止显示该面板。
- CPU 使用率通过统一延时探针估算。
- `src` 中直接使用的 `HAL_Delay` 曾统一替换为 `JPDelay`，`HAL_Delay` 应只留在 `libdly.c` 内部。

Confirm 组件已经支持自动换行，不能让文本越界覆盖按钮。

设置项还包括：

```text
Boot GFX <ON/OFF>
Logd on COM <ON/OFF>
Dashboard <ON/OFF>
Direction
Brightness
Auto Brightness
```

Boot GFX 的编辑状态是整条菜单背景蓝/灰闪动，不是只闪 ON/OFF。

## 十二、其他系统行为

- 离开 Launcher/Pet 页后，暂停来自 Launcher 的云端心跳、ACK、自动同步等网络推进；看门狗继续喂。回到页面后恢复。
- 首次时间同步在 WLAN 连接成功后、启动 logo 完成前尝试。
- HTTP 时间 fallback 不再使用失效的苏宁接口，改为解析普通网站响应的 `Date` header。
- 可恢复的网络/时间失败不再让黄灯常亮，而是黄灯闪约 300ms。
- 自动亮度、手动亮度和方向设置必须在 `PD_SplashFinish()` 后应用。
- TOS HID USB 启动时在 CubeMX USER CODE 区通过 PA12 断开脉冲强制重新枚举，避免必须手动插拔。

## 十三、构建与验收

主要构建：

```powershell
cd C:\Code\tos\slave-board
cmake --build build\Release
powershell -ExecutionPolicy Bypass -File scripts\export_partitions.ps1
```

每次修改 SBL/REC 后必须确认：

```text
SBL 正文没有进入状态 sector
REC 全部必要代码位于 REC 分区
SBL/REC 没有引用 SYSTEM 地址
四个 bin 大小与 partitions.csv 一致
system 擦除后仍可点屏、进入 FASTBOOT、使用 USB
USB 可以反复插拔重新识别
正常启动没有黑屏闪断
```

当前请不要先做无关 UI 优化。优先继续最后未完成的任务：

```text
修复 REC SD 卡挂载
-> 完整错误诊断
-> REC 复用统一动态内存
-> 实现独立 USB MSC
-> 验证热插拔
-> 验证 SD 卡刷 sah/system
```

这一版看似很完美，但是还有很多问题！
不纠结REC的USB识别问题了，修改策略为：主动进入REC模式快速启动USB挂载SD卡，需要依赖用户手动重启，其他情况不管USB状态，且其他情况跑完任务就复位，减少UI信息显示，改回之前样式！！！，不再停留（反正有日志，且要为以后远程升级做准备），REC还需要新增一个INIT模式；
（这一部分都是在TOS去实现检测）当TOS启动检查到SD卡已插入，确检测不到/init时即判定外部存储未初始化，需要进REC的INIT模式，即保存标志，触发复位进REC，建立基本的文件结构（参照TOS src\gui\miniapp\file_manager\app.cpp的初始化逻辑，把它源码和功能完整移动到REC分区来），并在每次TOS开机时检测文件系统（如果SD挂载正常）根目录非基本结构删除（这是TOS的部分）
还有，REC的格式化应该包括FLASH最尾端一段的用户数据的抹除，不用管SD卡的格式化，这个交给INIT模式，不一定SD卡每次都插着！而为了开发方便，我设置了宏定义，暂未让其真实格式化（结果你给我改了。。。）现在改回来

特别的，我发现SBL竟然被你修改的也在“喂狗”，这不能这样，SBL启动阶段利用寄存器直接关闭看门狗，等到进TOS后自己初始化，这一定要改！因为设备可以什么分区都没有，但一定得有SBL分区！！！确保其独立性！

再次审查各分区的独立性，确保system、rec、sah分区被我抹除后系统仍能启动sbl！！！

修改代码，用我给你的最新code包，基于这个修改，生成增量包！
