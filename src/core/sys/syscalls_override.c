#include "main.h"
#include <stdarg.h>
#include <stdio.h>


extern UART_HandleTypeDef huart1;

// 重定向 _write 函数
int _write(int file, char *ptr, int len) {
  // 忽略文件描述符，直接输出到 USART1
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

// 可选：重定向 _read 函数（如果需要输入）
int _read(int file, char *ptr, int len) {
  // 从 USART1 读取
  HAL_UART_Receive(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}