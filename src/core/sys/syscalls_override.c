/**
 ******************************************************************************
 * @file    syscalls_override.c
 * @author  Typheye
 * @brief   Syscalls Override implementation.
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

#include "include/syscalls_override.h"


extern UART_HandleTypeDef huart1;

/*
 * Debug UART must never stall the product, but it should also avoid cutting
 * logs while a serial monitor is attached.  Transmit in small chunks with a
 * bounded timeout derived from the byte count.  If the UART is unhealthy, drop
 * the rest of this log line and keep the application running.
 */
static void debug_uart_recover(void) {
  if (huart1.Instance == NULL) {
    return;
  }

  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE) != RESET) {
    __HAL_UART_CLEAR_OREFLAG(&huart1);
  }
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_NE) != RESET ||
      __HAL_UART_GET_FLAG(&huart1, UART_FLAG_FE) != RESET ||
      __HAL_UART_GET_FLAG(&huart1, UART_FLAG_PE) != RESET) {
    __HAL_UART_CLEAR_PEFLAG(&huart1);
  }

  if (huart1.ErrorCode != HAL_UART_ERROR_NONE) {
    huart1.ErrorCode = HAL_UART_ERROR_NONE;
  }
}

static uint32_t debug_uart_timeout_for_len(uint16_t n) {
  /* 115200 bps -> about 0.087 ms per byte at 10 bits/byte.
   * Add margin for HAL overhead. Keep a hard cap so production cannot hang.
   */
  uint32_t t = 6U + ((uint32_t)n + 7U) / 8U;
  if (t < 12U) t = 12U;
  if (t > 35U) t = 35U;
  return t;
}

int _write(int file, char *ptr, int len) {
  (void)file;
  if (ptr == NULL || len <= 0) {
    return 0;
  }

  if (huart1.Instance == NULL || huart1.gState == HAL_UART_STATE_RESET) {
    return len;
  }

  debug_uart_recover();

  int sent = 0;
  while (sent < len) {
    uint16_t chunk = (uint16_t)(len - sent);
    if (chunk > 96U) chunk = 96U;

    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1,
                                             (uint8_t *)(ptr + sent),
                                             chunk,
                                             debug_uart_timeout_for_len(chunk));
    if (st != HAL_OK) {
      (void)HAL_UART_AbortTransmit(&huart1);
      debug_uart_recover();
      break;
    }
    sent += chunk;
  }

  return len;
}

int _read(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  errno = EAGAIN;
  return 0;
}
