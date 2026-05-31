#ifndef ESP8266_HPP
#define ESP8266_HPP

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== C 兼容接口 ====================

/**
 * @brief 初始化 ESP8266
 */
void ESP8266_Init(void);

/**
 * @brief 处理接收到的数据（在中断中调用）
 * @param data 数据指针
 * @param len 数据长度
 */
void ESP8266_ProcessRxData(uint8_t *data, uint16_t len);

/**
 * @brief 发送命令
 * @param cmd 命令字符串
 * @param expected_response 期望的响应
 * @param timeout_ms 超时时间
 * @return true 成功, false 失败
 */
bool ESP8266_SendCommand(const char *cmd, const char *expected_response,
                         uint32_t timeout_ms);

/**
 * @brief 连接 WiFi
 * @param ssid WiFi SSID
 * @param password WiFi 密码
 * @return true 成功, false 失败
 */
bool ESP8266_ConnectWiFi(const char *ssid, const char *password);

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return true 成功, false 失败
 */
bool ESP8266_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief 启动 TCP 连接
 * @param host 主机地址
 * @param port 端口
 * @return true 成功, false 失败
 */
bool ESP8266_StartTCP(const char *host, uint16_t port);

/**
 * @brief 获取 ESP8266 状态
 * @return 0=断开, 1=连接中, 2=已连接, 3=已获取IP, 4=错误
 */
int ESP8266_GetState(void);

/**
 * @brief 检查是否已连接
 * @return true 已连接, false 未连接
 */
bool ESP8266_IsConnected(void);
void ESP8266_Disconnect(void);
bool ESP8266_GetIP(char *buf, uint16_t sz);

#ifdef __cplusplus
}
#endif

// ==================== C++ 类定义 ====================

#ifdef __cplusplus

class ESP8266 {
public:
  ESP8266(UART_HandleTypeDef *huart);

  void init(void);
  bool sendCommand(const char *cmd, const char *expected_response,
                   uint32_t timeout_ms);
  bool connectWiFi(const char *ssid, const char *password);
  bool sendData(const uint8_t *data, uint16_t len);
  bool startTCP(const char *host, uint16_t port);
  bool closeConnection(void);
  bool isConnected(void);
  int getState(void) { return _state; }

  // 数据处理（在中断中调用）
  void processRxData(uint8_t *data, uint16_t len);

  // 在 ESP8266 类中添加以下方法声明
  bool scanNetworks(void);
  void disconnect(void);
  bool getIP(char *ip_buffer, uint16_t buffer_size);
  bool sendString(const char *str);
  const char *getRxBuffer(void) const { return (const char *)_rx_buffer; }

  void processPendingData(void);
  void resetRxBuffer(void);

private:
  UART_HandleTypeDef *_huart;
  int _state; // 0=断开, 1=连接中, 2=已连接, 3=已获取IP
  uint8_t _rx_buffer[1536];
  uint16_t _rx_index;

  void clearRxBuffer(void);
  bool waitForResponse(const char *expected, uint32_t timeout_ms);
  void parseResponse(const char *response);
};

// 全局实例声明
extern ESP8266 esp8266;

#endif // __cplusplus

#endif // ESP8266_HPP
