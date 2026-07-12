# 更新记录

> 按时间倒序

---

## v1.0 (2026-07-12)

### 初始发布

- **ESP8266 AT 固件** `ESP8266-AT-1M.bin` v1.8.x 预编译镜像
  - 支持标准 AT 指令集（WiFi 连接、TCP Socket、OTA）
  - 适用于 ESP-12F 模块（1 MB Flash）

### 工具

- **Flash Download Tool v3.9.7**：乐鑫官方烧录工具
  - 预配置 ESP8266 SPI/HSPI 下载参数
  - 中英文 PDF 手册

- **AiThinker Serial Tool**：串口调试终端
  - 115200-8N1 默认配置
  - 用于 AT 固件功能验证

### 文档

- `docs/technical-docs.md`：硬件连接、AT 协议、STM32 驱动架构、故障恢复机制
- `README.md`：快速开始、目录结构、烧录/验证步骤
