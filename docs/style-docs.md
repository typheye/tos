# 代码风格

> 提取自 `src/` 目录，可直接用于其他仓库统一风格。

---

## 1. 文件头模板

每个 `.c` / `.cpp` / `.h` / `.hpp` 文件必须以这个模板开头：

```c
/**
 ******************************************************************************
 * @file    filename.ext
 * @author  Typheye
 * @brief   One-line description ending with period.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
```

规则：
- `@brief`：实现文件用 `XXX implementation.`，头文件用 `XXX interface.`
- 结尾带句号
- `@author` 固定为 `Typheye`

---

## 2. Include 守卫

**推荐风格**（新代码使用）：

```c
// .h 文件
#ifndef FILENAME_H
#define FILENAME_H
...
#endif /* FILENAME_H */

// .hpp 文件
#ifndef FILENAME_HPP
#define FILENAME_HPP
...
#endif // FILENAME_HPP
```

> ⚠️ 旧代码存在 `__FILENAME_H` 双下划线风格——这是 C 标准保留字，新代码不要使用。

---

## 3. 命名规范

### 文件名
全部 `snake_case`：`settings_manager.c`、`esp8266.hpp`、`wlan_activity.cpp`

### C 函数：`ModulePrefix_PascalCase()`
```c
void SysLog_Write(...);
void SM_Init(void);
void LCD_FillScreen(uint32_t color);
bool ESP8266_IsConnected(void);
FS_Status_t FS_Mount(const char *path);
```

### C 私有/静态函数：`snake_case()`
```c
static void copy_str(...);
static const char *syslog_level_name(...);
static int u32dec(uint32_t v, char *b);
```

### C++ 方法：`camelCase()`
```cpp
class LCD {
public:
  void fillScreen(uint32_t color);
  void drawPixel(uint16_t x, uint16_t y, uint32_t color);
  bool isInitialized(void);
};
```

### C++ 成员变量：`_camelCase`
```cpp
private:
  UART_HandleTypeDef *_huart;
  int _state;
  bool _hard_disabled;
  uint16_t _rx_index;
```

### 宏：`UPPER_SNAKE_CASE`
```c
#define LCD_WIDTH         240
#define SM_MAGIC          0x544F5304u
#define SYSLOG_MAX_LEVEL  SYSLOG_DEBUG
```

### 枚举类型：`PascalCase_t`
```c
typedef enum {
  SYSLOG_FATAL = 0,
  SYSLOG_ERROR = 1,
  SYSLOG_WARN  = 2,
  SYSLOG_INFO  = 3,
  SYSLOG_DEBUG = 4,
} SysLog_Level_t;

typedef enum {
  FS_OK = 0,
  FS_ERROR = 1,
  FS_NOT_MOUNTED = 2,
} FS_Status_t;
```

### 结构体 typedef：`PascalCase_t`
```c
typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  bool     disp_auto;
  uint8_t  disp_bright;
  char     wlan_ssid[24];
} Settings_t;
```

### C++ 类：`PascalCase`
```cpp
class ESP8266 { ... };
class KeyManager { ... };
```

---

## 4. Include 顺序

**.c / .cpp 文件**：自己的头文件永远排第一
```c
#include "include/self.h"      // 1. 自己
                               // 2. 空行
// ... 其他头文件 ...
#include <stdio.h>             // 3. 标准库
```

**.h / .hpp 文件**：必须自包含（所有依赖类型直接 include）

**跨模块引用**：从 `src/` 根目录出发
```c
#include "core/sys/include/syslog.h"
#include "hardware/include/lcd.h"
```

**自模块引用**：用 `include/` 相对路径
```c
#include "include/syslog.h"
#include "include/libfs.h"
```

---

## 5. `extern "C"` 模式

### .h 文件（纯 C 接口）：整段包裹
```c
#ifdef __cplusplus
extern "C" {
#endif

// 所有声明...

#ifdef __cplusplus
}
#endif
```

### .hpp 文件（C 接口 + C++ 类共存）：
```cpp
#ifdef __cplusplus
extern "C" {
#endif

// C wrapper 函数
void ESP8266_Init(void);
bool ESP8266_IsHardDisabled(void);

#ifdef __cplusplus
}
#endif

// C++ 类定义
#ifdef __cplusplus
class ESP8266 { ... };
extern ESP8266 esp8266;
#endif
```

### .h + .hpp 分离模式：
- `lcd.h`：C 兼容的函数声明 + 宏定义 → 可被 `.c` 和 `.cpp` 引用
- `lcd.hpp`：C++ 类定义 → 只被 `.cpp` 引用

### C 文件需要 C++ 声明时：条件 forward declare
```c
#ifdef __cplusplus
#include "hardware/include/esp8266.hpp"
#else
bool ESP8266_IsHardDisabled(void);   // C 可见的 forward declaration
#endif
```

---

## 6. 缩进与格式

- **缩进**：2 空格，不用 tab
- **大括号**：K&R 风格（左括号不换行）
```c
void func(void) {
  if (cond) {
    // ...
  } else {
    // ...
  }
}
```
- **关键字后加空格**：`if (`, `while (`, `for (`
- **函数调用不加空格**：`func(...)`
- **指针**：`Type *name`（星号靠变量名）

---

## 7. 注释

### Doxygen（公开 API）：
```c
/**
 * @brief  Do something useful.
 * @param  param_name  Description.
 * @return true on success, false on failure.
 */
```

### 分段标记：
```c
/* ========== Section name ========== */

/* ── Sub-section ── */

// ── C++ 文件也可用这种方式 ──
```

### 行内注释：`/* */` 或 `//` 均可，保持上下文一致

### 语言：英文

---

## 8. 日志

### 模块标签（5 字符，不足则右补空格）：
```
MAIN, SMGR, LCD, ESP, FS,  SYS,  RTC, SDIO,
KEY,  LED,  BUZZ, WLAN, HOTS, DSPL, STOR, PET,
EHW,  PD,   KB,   DEMO, FLASH
```

### 短格式（推荐）：
```c
LOG_I("MAIN", "System initialized, CPU:168MHz");
LOG_W("MAIN", "SD card is hard-disabled");
LOG_E("SMGR", "Load failed: %d", err);
LOG_D("LCD",  "brightness=%u", val);
```

### 长格式（带 task 字段，旧代码）：
```c
LOG_INFO("MAIN", "init", "System initialized");
```

### 编译期过滤：
`LOG_I` / `LOG_D` 受 `SYSLOG_MAX_LEVEL` 控制；`LOG_F` / `LOG_E` / `LOG_W` 始终编译。

---

## 9. CCMRAM 使用

```c
#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

static CCMRAM FATFS fs;
static CCMRAM Settings_t g_settings;
```

> ⚠️ CCMRAM **不能被 DMA 访问**（SDIO、SPI DMA 等），DMA buffer 绝对不能放 CCMRAM。

---

## 10. 目录结构

```
module_name/
  include/           ← 头文件放这里
    module.h
    module.hpp
  module.c           ← 实现文件与 include/ 同级
  module.cpp
```

**跨模块引用**：`"other_module/include/header.h"`（从 `src/` 根目录）
**自模块引用**：`"include/header.h"`（从实现文件所在目录）

---

## 快速检查清单

- [ ] 文件头模板完整
- [ ] Include 守卫格式正确（无 `__` 前缀）
- [ ] C 函数 `Module_Action()`，C++ 方法 `camelCase()`
- [ ] C++ 成员变量 `_prefix`
- [ ] 自己的头文件第一个 include
- [ ] `extern "C"` 正确包裹
- [ ] 2 空格缩进，K&R 大括号
- [ ] 日志用短格式 + 5 字符模块标签
- [ ] DMA buffer 不放 CCMRAM
- [ ] 英文注释
