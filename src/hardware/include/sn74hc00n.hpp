/**
 ******************************************************************************
 * @file    sn74hc00n.hpp
 * @author  Typheye
 * @brief   Sn74Hc00N interface.
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

#ifndef SN74HC00N_HPP
#define SN74HC00N_HPP

#include "main.h"
#include <stdint.h>
#include "core/sys/include/syslog.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif



#define HC00N_OUT1_PORT GPIOE
#define HC00N_OUT1_PIN GPIO_PIN_13 // 1Y

#define HC00N_OUT2_PORT GPIOE
#define HC00N_OUT2_PIN GPIO_PIN_12 // 2Y

#define HC00N_OUT3_PORT GPIOE
#define HC00N_OUT3_PIN GPIO_PIN_11 // 3Y

#define HC00N_OUT4_PORT GPIOE
#define HC00N_OUT4_PIN GPIO_PIN_10 // 4Y


#define HC00N_HIGH 1
#define HC00N_LOW 0


#define SW1A_MASK 0x01
#define SW1B_MASK 0x02
#define SW2A_MASK 0x04
#define SW2B_MASK 0x08
#define SW3A_MASK 0x10
#define SW3B_MASK 0x20
#define SW4A_MASK 0x40
#define SW4B_MASK 0x80


typedef struct {
  uint8_t sw1A;
  uint8_t sw1B;
  uint8_t sw2A;
  uint8_t sw2B;
  uint8_t sw3A;
  uint8_t sw3B;
  uint8_t sw4A;
  uint8_t sw4B;
} HC00N_Switches_t;

typedef struct {
  uint8_t output1;
  uint8_t output2;
  uint8_t output3;
  uint8_t output4;
} HC00N_Outputs_t;


class SN74HC00N {
public:
  SN74HC00N();


  void init(void);
  bool isInitialized(void) { return _initialized; }


  uint8_t readOutputByte(void);
  HC00N_Outputs_t readOutputs(void);
  uint8_t readOutput(uint8_t channel);


  static uint8_t nandGate(uint8_t a, uint8_t b);
  static uint8_t calculateNANDOutput(uint8_t switches);


  void updateFromSwitches(uint8_t switchStates);


  HC00N_Switches_t getSwitchStates(void);
  HC00N_Outputs_t getActualOutputs(void);


  bool verifyOutputs(uint8_t switches);


  void debugPrint(void);

private:
  bool _initialized;
  uint8_t _lastSwitches;
  HC00N_Outputs_t _lastOutputs;
};

extern SN74HC00N boardHC00N;

#ifdef __cplusplus
}
#endif

#endif /* SN74HC00N_HPP */