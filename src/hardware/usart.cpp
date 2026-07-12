/**
 ******************************************************************************
 * @file    usart.cpp
 * @author  Typheye
 * @brief   Usart implementation.
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