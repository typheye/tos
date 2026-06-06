# TOS 日志系统文档

---

## 1. 架构总览

```
┌──────────────────────────────────────────────────────────────┐
│  调用方 (任意 .c / .cpp)                                      │
│  LOG_I("MAIN", "System initialized, CPU:%dMHz", 168);        │
└──────────────────────┬───────────────────────────────────────┘
                       │ 宏展开
                       ▼
              SysLog_Print(level, mod, fmt, ...)
                       │
                       ▼
              syslog_emit_v()   ─── 内部函数 ──────────────┐
                       │                                    │
          ┌────────────┼────────────┐                       │
          ▼            ▼            ▼                       │
     syslog_ts()   vsnprintf   printf()                     │
     时间戳生成     格式化消息    串口输出                    │
                                   │                        │
                     ┌─────────────┘                        │
                     ▼                                      │
              level <= SYSLOG_FILE_MAX_LEVEL ?              │
                     │                                      │
              ┌──────┴──────┐                               │
              ▼              ▼                               │
             是             否 → 结束                        │
              │                                              │
              ▼                                              │
     FMCore_AppendBootLog()  ─── file_manager.c ────────────┤
              │                                              │
              ▼                                              │
         SD 卡 /system/log/00000001.log                      │
└────────────────────────────────────────────────────────────┘
```

日志系统由两个模块组成：

| 模块 | 文件 | 职责 |
|------|------|------|
| **syslog** | `src/core/sys/syslog.h` / `.c` | 日志等级、格式化、串口输出、写入调度 |
| **file_manager** | `src/core/manager/file_manager.h` / `.c` | SD 卡挂载、目录管理、日志文件创建与追加 |

---

## 2. 日志等级

```c
typedef enum {
  SYSLOG_FATAL = 0,   // 系统不可用，即将复位
  SYSLOG_ERROR = 1,   // 错误条件
  SYSLOG_WARN  = 2,   // 警告条件
  SYSLOG_INFO  = 3,   // 一般信息（默认文件输出最高等级）
  SYSLOG_DEBUG = 4,   // 调试信息
} SysLog_Level_t;
```

| 等级 | 数值 | 串口输出 | 文件输出 | 典型场景 |
|------|------|----------|----------|----------|
| FATAL | 0 | ✅ 始终 | ✅ 始终 | 系统崩溃前的最后一条日志 |
| ERROR | 1 | ✅ 始终 | ✅ 始终 | SD 卡写入失败、外设初始化失败 |
| WARN | 2 | ✅ 始终 | ✅ 始终 | SD 卡被禁用、WiFi 连接失败 |
| INFO | 3 | ✅ 始终 | ✅ 始终 | 系统初始化完成、WiFi 连接成功 |
| DEBUG | 4 | 编译期可选 | ❌ 默认不写 | 外设状态轮询、帧率统计 |

---

## 3. 编译期配置

所有开关都在 `syslog.h` 中，可在项目配置中覆盖：

```c
// 串口输出等级阈值（默认全部输出）
#ifndef SYSLOG_MAX_LEVEL
#define SYSLOG_MAX_LEVEL  SYSLOG_DEBUG    // 改为 SYSLOG_INFO 可关闭 DEBUG
#endif

// 文件输出等级阈值（默认 INFO 及以上才写 SD 卡）
#ifndef SYSLOG_FILE_MAX_LEVEL
#define SYSLOG_FILE_MAX_LEVEL SYSLOG_INFO
#endif
```

**编译行为**：
- `LOG_INFO()` / `LOG_DEBUG()` 在 `SYSLOG_MAX_LEVEL` 不足时被编译为 `((void)0)`，零开销
- `LOG_F()` / `LOG_E()` / `LOG_W()` / `LOG_I()` / `LOG_D()` 短格式宏**始终编译**，不受 `SYSLOG_MAX_LEVEL` 控制
- 文件写入在 `level > SYSLOG_FILE_MAX_LEVEL` 时跳过（默认 DEBUG 不写文件）

### 推荐配置

```c
// 开发调试阶段 —— 全开
#define SYSLOG_MAX_LEVEL       SYSLOG_DEBUG
#define SYSLOG_FILE_MAX_LEVEL  SYSLOG_DEBUG

// 生产发布 —— 关闭 DEBUG
#define SYSLOG_MAX_LEVEL       SYSLOG_INFO
#define SYSLOG_FILE_MAX_LEVEL  SYSLOG_INFO
```

---

## 4. 输出格式

### 4.1 串口格式（UART）

```
[sssss.mmm] [LEVEL] [MOD  ] message text here\r\n
```

字段解析：

```
[  331.702] [DEBUG] [EHW  ] imu acc=1943 lin=1 gyro=0 rawg=0 tilt=0 rate=0 rest=1
│           │       │        │
│           │       │        └─ 日志正文（printf 格式化）
│           │       └─ 模块标签（5 字符，右补空格）
│           └─ 等级名（5 字符，右补空格：FATAL/ERROR/WARN /INFO /DEBUG）
└─ 时间戳（启动后的毫秒数，详见 §4.2）
```

### 4.2 时间戳格式

时间戳来自 `HAL_GetTick()`（启动后的毫秒计数），使用固定 8 位数字预算自动适应：

| 运行时间 | 格式 | 示例 |
|----------|------|------|
| 0 ~ 9 秒 | `[    s.mmm]` | `[    1.149]` |
| 10 ~ 99 秒 | `[   ss.mmm]` | `[   53.420]` |
| 100 ~ 999 秒 | `[  sss.mmm]` | `[  331.702]` |
| 1000 ~ 9999 秒 | `[ ssss.mmm]` | `[ 5432.100]` |
| 10000 ~ 99999 秒 | `[sssss.mmm]` | `[12345.678]` |
| 100000 秒+ | `[ssssss.mm]` | `[123456.78]` → 毫秒精度递减 |

> 设计目标：中括号内的数字部分始终保持 8 位，小数点随秒数增大而右移。

**时间戳生成函数** (`syslog_ts()`)：
- 自实现 `u32dec()` 整数转十进制，**避免 newlib-nano 的 `snprintf` bug**（nano 版本不支持 `%u`）
- 返回静态 buffer，**非线程安全、非重入**（系统单线程，无影响）

### 4.3 文件格式

写入 SD 卡的文件内容与串口输出**完全一致**：`[timestamp] [LEVEL] [MOD  ] message\r\n`

文件路径：`0:/system/log/NNNNNNNN.log`（8 位递增序号）

---

## 5. 日志宏速查

### 5.1 短格式（推荐）

```c
LOG_F("MAIN", "Fatal error: 0x%08lX", code);   // FATAL — 始终编译
LOG_E("SMGR", "Load failed: %d", err);           // ERROR — 始终编译
LOG_W("MAIN", "SD card is hard-disabled");       // WARN  — 始终编译
LOG_I("MAIN", "System initialized, CPU:%dMHz");  // INFO  — 始终编译
LOG_D("LCD",  "brightness=%u", val);             // DEBUG — 始终编译
```

参数：`(mod, fmt, ...)` — 模块标签 + printf 格式 + 可变参数

### 5.2 长格式（带 task/context）

```c
LOG_FATAL("MAIN", "init", "Fatal error: 0x%08lX", code);
LOG_ERROR("MAIN", "init", "Load failed: %d", err);
LOG_WARN ("MAIN", "init", "SD card is hard-disabled");
LOG_INFO ("MAIN", "init", "System initialized");       // 受 SYSLOG_MAX_LEVEL >= 3 控制
LOG_DEBUG("MAIN", "init", "brightness=%u", val);       // 受 SYSLOG_MAX_LEVEL >= 4 控制
```

参数：`(mod, task, fmt, ...)` — 多了一个 task/context 字段（当前被忽略，保留用于未来扩展）

### 5.3 底层函数

```c
// 格式化并输出（短格式宏最终调用此函数）
void SysLog_Print(SysLog_Level_t level, const char *mod, const char *fmt, ...);

// 完整版（长格式宏调用，task/file/line 当前被忽略）
void SysLog_Write(SysLog_Level_t level, const char *mod, const char *task,
                  const char *file, int line, const char *fmt, ...);

// 写入系统崩溃 dump 到 SD 卡
void SysLog_WriteFatalDump(uint32_t code, const char *name);

// 禁用/查询 SD 卡文件输出
void SysLog_DisableFileOutput(void);
bool SysLog_IsFileOutputDisabled(void);

// 获取系统 tick（weak，可重写）
uint32_t SysLog_GetTick(void);
```

---

## 6. 模块标签

每个日志必须带一个 5 字符的模块标签（不足 5 字符时右补空格）：

| 标签 | 模块 | 说明 |
|------|------|------|
| `MAIN ` | main / TOS init | 主初始化流程 |
| `SMGR ` | settings manager | 设置管理 |
| `FMCR ` | file manager core | SD 卡文件管理核心 |
| `FS   ` | file system | 文件系统操作 |
| `FLASH` | sfhd Flash driver | 内部 Flash 驱动 |
| `RTC  ` | real-time clock | 实时时钟 |
| `SDIO ` | SD card driver | SD 卡硬件驱动 |
| `ESP  ` | ESP8266 WiFi | WiFi 模块 |
| `LCD  ` | display driver | 显示屏驱动 |
| `TCS  ` | TCS3472 | 颜色传感器 |
| `BMP  ` | BMP180 | 气压传感器 |
| `JYRO ` | JY901S | 陀螺仪 |
| `POT  ` | potentiometer | 电位器 |
| `HC00 ` | SN74HC00N | 逻辑芯片 |
| `KEY  ` | key manager | 按键管理 |
| `LED  ` | LED driver | LED 驱动 |
| `BUZZ ` | buzzer | 蜂鸣器 |
| `WLAN ` | WiFi settings | WiFi 设置界面 |
| `HOTS ` | hotspot settings | 热点设置界面 |
| `DSPL ` | display settings | 显示设置界面 |
| `STOR ` | storage settings | 存储设置界面 |
| `SND  ` | sound settings | 声音设置界面 |
| `TIME ` | time settings | 时间设置界面 |
| `PET  ` | launcher / pet | 桌面宠物 |
| `EHW  ` | expression hardware | 表情硬件层 |
| `PD   ` | libpd display | 显示库 |
| `KB   ` | keyboard | 键盘组件 |
| `SYS  ` | syscalls/system | 系统级 |
| `TAPI ` | TOS API | 云端 API 调用 |
| `NET  ` | network | 网络传输层 |
| `DEMO ` | demo activities | Demo 页面 |
| `I2C  ` | I2C scanner | I2C 扫描器 |
| `FM   ` | file manager app | 文件管理器小程序 |

---

## 7. SD 卡文件存储

### 7.1 目录结构

```
SD 卡根目录:
  /init                     ← 文件系统就绪标记（23 字节）
  /data/                    ← 用户数据
  /oem/                     ← OEM 配置
  /dev/                     ← 开发数据
  /storage/                 ← 存储
  /system/                  ← 系统目录
    /log/                   ← 启动日志
      00000001.log
      00000002.log
      ...
    /dump/                  ← 系统崩溃 dump
      00000001.dump
      00000002.dump
      ...
```

### 7.2 启动日志生命周期

```
系统启动
  │
  ├─ SD 卡未就绪 → 跳过，只输出串口
  │
  └─ SD 卡就绪
       │
       ├─ 确保 /system/log/ 目录存在
       ├─ 扫描已有日志文件，找到下一个可用序号（00000001 ~ 99999999）
       ├─ 创建新日志文件 0:/system/log/NNNNNNNN.log
       │
       └─ 每次 LOG_xxx() 调用：
            ├─ 格式化日志行
            ├─ printf() 输出到串口
            └─ 追加写入当前日志文件
                 │
                 ├─ 写入成功 → 继续
                 ├─ 写入超时（3.5 秒）→ 标记 fatal_fault，后续跳过
                 └─ 写入失败 → 触发 SysHandle_Exception (SD_DISK_ERR)
```

### 7.3 写入保护机制

```c
#define FMCORE_BOOT_LOG_TIMEOUT_MS  3500U   // 单次写入超时（3.5 秒）

static uint8_t g_syslog_file_guard = 0;    // 重入保护（防止日志递归）
static uint8_t g_syslog_file_disabled = 0; // 永久禁用标记
static uint8_t g_syslog_storage_faulting = 0; // 故障处理中（防死循环）
```

- **重入保护**：日志写入期间 `g_syslog_file_guard = 1`，如果在日志回调中再次打日志，跳过文件写入
- **故障保护**：如果 SD 卡写入失败，标记 `g_syslog_file_disabled = 1`，后续只输出串口
- **超时保护**：每次 `f_open/f_write/f_sync/f_close` 都检查累计时间，超过 3.5 秒立即放弃
- **看门狗**：每次文件操作前后调用 `SysWatchdog_FeedNow()`，防止长写入触发 IWDG

---

## 8. 系统崩溃 Dump

### 8.1 触发路径

```
致命错误发生
  │
  ├─ 方式 1：SysHandle_Exception(code)
  │           └─ 调用 SysLog_WriteFatalDump(code, name)
  │
  └─ 方式 2：SD 卡日志写入失败（连续多次）
              └─ syslog_handle_file_result()
                   └─ 调用 SysHandle_ExceptionNoDump(code)
                        └─ 跳过 dump 写入，直接显示错误 UI
```

### 8.2 Dump 文件格式

```
TOS system dump
code=0x00001003
type=SD_DISK_ERR
tick_ms=12345
rtc=2026-06-06 14:30:00 wd=6
esp_state=2
esp_hard_disabled=0
sdio_initialized=1
sdio_hard_disabled=0
```

包含：错误码、类型名、运行毫秒数、RTC 时间、ESP8266 状态、SDIO 状态

---

## 9. 错误码体系

错误码使用 `0x00SS00NN` 格式（SS=子系统，NN=错误序号）：

| 子系统 | 错误码范围 | 示例 |
|--------|-----------|------|
| SD 卡 | `0x000010XX` | `SYS_ERR_SD_DISK_ERR = 0x00001003` |
| UI | `0x000020XX` | `SYS_ERR_UI_STORAGE_PROBE = 0x00002001` |
| ESP8266 | `0x000030XX` | `SYS_ERR_ESP8266_AT_TIMEOUT = 0x00003001` |
| 系统 | `0x000040XX` | `SYS_ERR_IWDG_RESET = 0x00004002` |

错误码值保持稳定，用于崩溃界面显示和后续日志分析。

---

## 10. 内部实现细节

### 10.1 格式化缓冲区

```c
static void syslog_emit_v(SysLog_Level_t level, const char *mod,
                          const char *fmt, va_list ap) {
  char msg[224];    // 格式化后的消息体（最大 223 字符 + \0）
  char line[320];   // 完整日志行（时间戳 + 等级 + 模块 + 消息 + \r\n）
  ...
}
```

- `msg[224]`：消息体上限 223 字符，超出部分截断
- `line[320]`：完整行上限 318 字符 + \r\n，等于串口输出的一行最大长度
- 时间戳占用 10 字符 (`[sssss.mmm]`)，等级 6 字符 (`[ERROR]`)，模块 7 字符 (`[MOD  ]`)，开销共 ~23 字符
- 消息体实际可用约 290 字符（如果 line 全部利用）

### 10.2 newlib-nano 兼容性

嵌入式工具链使用 `newlib-nano`，其 `snprintf` 对某些格式说明符支持不完整。为规避此问题：
- 时间戳使用自实现 `u32dec()` 替代 `snprintf(..., "%lu", ...)`
- 日志宏使用 `printf()` 直接输出（而非通过 `snprintf` 中转）

### 10.3 FatFs 配置

```c
#define _USE_LFN     2    // 长文件名：栈上动态 buffer
#define _MAX_LFN     64   // 最长文件名 64 字符（路径 192 字符）
#define _LFN_UNICODE 0    // ANSI/OEM 编码
#define _STRF_ENCODE 3    // UTF-8
```

### 10.4 SysLog_GetTick 的可重写性

```c
__attribute__((weak))
uint32_t SysLog_GetTick(void) { return HAL_GetTick(); }
```

`weak` 属性允许在其他编译单元中重写此函数（例如测试环境中注入模拟 tick）。

---

## 11. 使用示例

### 11.1 基本使用

```c
#include "core/sys/include/syslog.h"

void my_function(void) {
  LOG_I("MAIN", "System initialized, CPU:%dMHz", 168);

  if (error_condition) {
    LOG_E("MAIN", "Fatal error: 0x%08lX", error_code);
    return;
  }

  LOG_D("MAIN", "counter=%u state=%d", counter, state);
}
```

### 11.2 条件编译（节省 flash）

```c
// 生产构建时，DEBUG 日志被完全移除
LOG_D("MAIN", "This is removed in release builds");

// 始终输出的日志
LOG_I("MAIN", "This always appears");
```

### 11.3 禁用 SD 卡文件输出

```c
// 在 SD 卡不可用的场景下
SysLog_DisableFileOutput();
LOG_I("MAIN", "Running without SD card");  // 只输出串口
```

### 11.4 写入系统 Dump

```c
// 在致命错误处理中
SysLog_WriteFatalDump(0x00001003, "SD_DISK_ERR");
// 会生成 /system/dump/00000001.dump 文件
```

---

## 12. 注意事项

1. **短格式宏不检查等级**：`LOG_I()` / `LOG_D()` 始终编译执行（仅文件写入受 `SYSLOG_FILE_MAX_LEVEL` 控制）。频繁调用的 DEBUG 日志建议用长格式 `LOG_DEBUG()` 或手动 `#if` 包裹
2. **单线程设计**：`syslog_ts()` 返回静态 buffer，不可重入。系统为单线程事件循环，无并发问题
3. **重入保护**：日志回调中再次打日志，文件写入被跳过（`g_syslog_file_guard`），但串口仍输出
4. **SD 卡不可用时**：日志只输出串口，文件写入静默跳过（`FMCore_IsInitialized()` 返回 false）
5. **启动日志文件序号**：每次上电创建新文件，序号递增，上限 99999999 后返回 `FR_DENIED`
6. **写入超时**：每次文件操作限时 3.5 秒总耗时，超时后 `fatal_fault = true`，当前启动周期内停止写入
