# 技术手册

> 模块: ESP8266 (ESP-12F) | 固件: AT v1.8.x | 接口: UART (115200-8N1)

---

## 1. 项目概述

本仓库提供 TOS 系统中 ESP8266 WiFi 模块所需的 **AT 固件**（预编译二进制）。ESP8266 作为 **串口转 WiFi 桥接器**，通过 UART2 与 STM32F407 主控通信，使用标准的乐鑫 AT 指令集。烧录工具和串口终端请从各厂商官方渠道获取。

### 在 TOS 系统中的位置

```
TOS Slave Board (STM32F407)
  │
  ├── UART1 (PA9/PA10) — 调试串口 (115200-8N1)
  │
  └── UART2 (PD5/PD6) — ESP8266 通信 (115200-8N1)
        │
        ▼
    ESP8266 (ESP-12F) ─── WiFi ──── 互联网
        │
        ├── EN (GPIOF0)  ─── 模块使能控制
        └── RST (GPIOF1) ─── 模块复位控制
```

---

## 2. 硬件连接

| STM32F407 引脚 | ESP8266 引脚 | 说明 |
|---------------|-------------|------|
| PD5 (UART2 TX) | RXD | STM32 → ESP8266 |
| PD6 (UART2 RX) | TXD | ESP8266 → STM32 |
| PF0 | EN (CH_PD) | 模块使能（高电平有效） |
| PF1 | RST | 模块复位（低电平复位） |
| 3.3V | VCC | 供电 |
| GND | GND | 共地 |

---

## 3. AT 指令协议

ESP8266 使用标准 AT 指令集。所有指令以 `\r\n` 结尾，响应以 `\r\n` 分隔。

### 常用指令

| 指令 | 说明 | 期望响应 |
|------|------|---------|
| `AT` | 测试连接 | `OK` |
| `AT+RST` | 复位模块 | `ready` |
| `ATE0` | 关闭回显 | `OK` |
| `AT+CWJAP="ssid","pwd"` | 连接 WiFi | `OK` / `FAIL` |
| `AT+CWJAP?` | 查询当前 AP | `+CWJAP:"ssid"` |
| `AT+CIFSR` | 获取 IP 地址 | `+CIFSR:STAIP,"192.168.x.x"` |
| `AT+CIPSTART="TCP","host",port` | 建立 TCP 连接 | `CONNECT` |
| `AT+CIPSEND=<len>` | 发送数据（长度） | `>` 后跟数据 |
| `AT+CIPCLOSE` | 关闭连接 | `CLOSED` |
| `AT+CIPMODE=0` | 设置正常模式 | `OK` |
| `AT+CIPMUX=0` | 单连接模式 | `OK` |
| `AT+CWLAP` | 扫描可用 AP | `+CWLAP:(...)` |

### 通信流程

```
1. 硬件复位（EN 拉低 → 拉高 → RST 脉冲）
2. 等待启动（~2.8s）
3. AT 测试 → ATE0 → AT+CIPMODE=0 → AT+CIPMUX=0
4. AT+CWJAP="ssid","pwd"       ← 连接 WiFi（最长 15s）
5. AT+CIPSTART="TCP","h",port  ← 建立 TCP 连接
6. AT+CIPSEND=<len>            ← 发送数据
7. AT+CIPCLOSE                 ← 关闭连接
```

---

## 4. STM32 驱动架构

驱动层位于 `slave-board` 的 `src/hardware/` 目录下，采用 **C 兼容接口 + C++ 类** 双层结构。

### C 接口（`esp8266.h` — 可被 `.c` 和 `.cpp` 引用）

```c
void ESP8266_Init(void);
bool ESP8266_IsConnected(void);
bool ESP8266_IsHardDisabled(void);
bool ESP8266_SendCommand(const char *cmd, const char *expected, uint32_t timeout);
bool ESP8266_ConnectWiFi(const char *ssid, const char *password);
bool ESP8266_SendData(const uint8_t *data, uint16_t len);
bool ESP8266_StartTCP(const char *host, uint16_t port);
void ESP8266_Disconnect(void);
bool ESP8266_GetIP(char *buf, uint16_t sz);
bool ESP8266_GetRSSI(int *rssi);
bool ESP8266_TryRecover(bool force);
void ESP8266_ServiceUartRx(void);
uint16_t ESP8266_GetRecoveryFailureCount(void);
```

### C++ 类（`esp8266.hpp` — 内部实现）

```cpp
class ESP8266 {
  void init(void);
  bool sendCommand(const char *cmd, const char *expected, uint32_t timeout_ms);
  bool connectWiFi(const char *ssid, const char *password);
  bool sendData(const uint8_t *data, uint16_t len);
  bool startTCP(const char *host, uint16_t port);
  bool isConnected(void);
  int getState(void);       // 0=disconnected, 3=got IP
  bool isHardDisabled(void);
  bool tryRecover(bool force);
  void serviceUartRx(void);
  // ... private methods ...
};
```

### 接收缓冲

```
RX_BUFFER[4096] — 环形缓冲区，ISR 写入，主循环消费
  ├── UART RX 中断 → processRxData()  → 写入 _rxBuffer
  ├── 主循环 → processPendingData() → 解析 AT 响应
  └── 溢出保护 → _rxOverflow 标志 → 清空重试
```

---

## 5. 故障恢复机制

ESP8266 驱动实现了多层故障恢复，确保 WiFi 模块在异常后能自动恢复。

### 恢复流程

```
异常检测（UART RX 停滞 → ORE/NE 错误 → RXNE 关闭）
  │
  ├── 1. 软件恢复：UART 重新使能 RXNE/EIE 中断
  │    └── 成功 → 继续运行
  │
  └── 2. 硬件复位（EN/RST 引脚控制）
       ├── 拉低 EN + RST → 等待 250ms
       ├── 拉高 EN → 等待 120ms
       ├── 拉高 RST → 等待 ~3.2s 启动
       └── AT 测试
            ├── 成功 → 恢复运行
            └── 失败 → 标记 _hardDisabled = true
```

### 恢复速率限制

- 两次自动恢复之间至少间隔 60s（`_last_recover_ms`）
- 恢复尝试计数（`_recover_attempts`）上限 255
- 连续失败后标记 `_hardDisabled`，后续操作直接跳过
- 外部可通过 `ESP8266_TryRecover(true)` 强制恢复

---

## 6. 固件烧录

### 固件：`firmware/ESP8266-AT-1M.bin`

- 来源：乐鑫官方发布的 ESP8266 AT 固件 v1.8.x
- 适用型号：ESP8266-ESP-12F（1 MB Flash 版本）
- 烧录地址：`0x0`
- SPI 模式：DOUT, 40 MHz
- 若需自定义编译，可从 [乐鑫 ESP-AT 项目](https://github.com/espressif/esp-at) 获取源码

### 烧录工具

使用 [Espressif Flash Download Tool](https://www.espressif.com/en/support/download/other-tools) 烧录固件：

1. 将 ESP8266 的 GPIO0 拉低进入下载模式
2. 连接 USB-TTL 转换器（TX→RX, RX→TX, GND→GND）
3. 打开 Flash Download Tool，选择 **ESP8266** > **SPI Download**
4. 配置参数：SPI Speed 40 MHz, Mode DOUT, Flash Size 16Mbit, 地址 `0x0`
5. 选择 `ESP8266-AT-1M.bin`，点击 START

### 验证

烧录后使用任意串口终端工具（如 PuTTY、[AiThinker Serial Tool](https://docs.ai-thinker.com/)）验证：

```text
打开 COM 口 115200-8N1
发送: AT
期望: OK

发送: AT+GMR
期望: AT version:1.8.x...
```

---

## 7. 日志与诊断

ESP8266 模块在 STM32 侧的日志模块标签为 `ESP`，日志等级：

| 级别 | 场景 |
|------|------|
| `LOG_E("ESP", ...)` | 恢复失败、RX 溢出、命令超时 |
| `LOG_W("ESP", ...)` | UART 重装、硬件复位、恢复尝试 |
| `LOG_I("ESP", ...)` | 初始化完成、WiFi 连接、IP 获取 |
| `LOG_D("ESP", ...)` | RX 缓冲区状态、UART 计数 |

---

## 8. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v1.0 | 2026-07-12 | 初始发布：ESP8266 AT 固件 v1.8.x |

详细更新记录见 [update-docs.md](update-docs.md)。
