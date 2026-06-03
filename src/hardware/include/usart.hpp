/**
 ******************************************************************************
 * @file    usart.hpp
 * @author  Typheye
 * @brief   Usart interface.
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

#ifndef __USART_HPP
#define __USART_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include <cstring>
#include <stdio.h>

class USART {
public:
  USART(USART_TypeDef *instance);
  void init();
  void send(uint8_t *data, uint16_t size);
  void send(const char *str);
  uint8_t receive(uint32_t timeout = HAL_MAX_DELAY);
  uint16_t receive(uint8_t *buffer, uint16_t max_size,
                   uint32_t timeout = HAL_MAX_DELAY);
  bool available();
  UART_HandleTypeDef *getHandle() { return &_huart; }

private:
  USART_TypeDef *_instance;
  UART_HandleTypeDef _huart;
  bool _initialized;
};

extern USART boardSerial;

#endif // __USART_HPP