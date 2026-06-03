/**
 ******************************************************************************
 * @file    usart.cpp
 * @author  Typheye
 * @brief   Usart implementation.
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

#include "include/usart.hpp"


extern UART_HandleTypeDef huart1;

#define _huart huart1

USART boardSerial(USART1);


USART::USART(USART_TypeDef *instance)
    : _instance(instance), _initialized(false) {}


void USART::init() {
  if (_initialized)
    return;

  _initialized = true;
}


void USART::send(uint8_t *data, uint16_t size) {
  if (!_initialized)
    init();
  HAL_UART_Transmit(&_huart, data, size, 12U);
}


void USART::send(const char *str) {
  if (!_initialized)
    init();
  uint16_t len = strlen(str);
  HAL_UART_Transmit(&_huart, (uint8_t *)str, len, 12U);
}


uint8_t USART::receive(uint32_t timeout) {
  if (!_initialized)
    init();
  uint8_t data;
  HAL_UART_Receive(&_huart, &data, 1, timeout);
  return data;
}


uint16_t USART::receive(uint8_t *buffer, uint16_t max_size, uint32_t timeout) {
  if (!_initialized)
    init();
  HAL_UART_Receive(&_huart, buffer, max_size, timeout);
  return max_size;
}


bool USART::available() {
  if (!_initialized)
    init();
  return __HAL_UART_GET_FLAG(&_huart, UART_FLAG_RXNE) != RESET;
}