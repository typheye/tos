/**
 ******************************************************************************
 * @file    usart.hpp
 * @author  Typheye
 * @brief   Usart interface.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef USART_HPP
#define USART_HPP

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

#endif // USART_HPP