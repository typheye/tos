#include "include/usart.hpp"

extern UART_HandleTypeDef huart1;

#define _huart huart1

USART boardSerial(USART1);

// 构造函数
USART::USART(USART_TypeDef *instance)
    : _instance(instance), _initialized(false) {}

// 初始化
void USART::init() {
  if (_initialized)
    return;

  _initialized = true;
}

// 发送数据（字节数组）
void USART::send(uint8_t *data, uint16_t size) {
  if (!_initialized)
    init();
  HAL_UART_Transmit(&_huart, data, size, 12U);
}

// 发送数据（字符串）- 这个必须实现！
void USART::send(const char *str) {
  if (!_initialized)
    init();
  uint16_t len = strlen(str);
  HAL_UART_Transmit(&_huart, (uint8_t *)str, len, 12U);
}

// 接收单个字节
uint8_t USART::receive(uint32_t timeout) {
  if (!_initialized)
    init();
  uint8_t data;
  HAL_UART_Receive(&_huart, &data, 1, timeout);
  return data;
}

// 接收多个字节
uint16_t USART::receive(uint8_t *buffer, uint16_t max_size, uint32_t timeout) {
  if (!_initialized)
    init();
  HAL_UART_Receive(&_huart, buffer, max_size, timeout);
  return max_size;
}

// 检查是否有数据可读
bool USART::available() {
  if (!_initialized)
    init();
  return __HAL_UART_GET_FLAG(&_huart, UART_FLAG_RXNE) != RESET;
}