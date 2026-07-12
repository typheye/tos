/**
 ******************************************************************************
 * @file    sn74hc00n.cpp
 * @author  Typheye
 * @brief   Sn74Hc00N implementation.
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

#include "include/sn74hc00n.hpp"



SN74HC00N boardHC00N;


SN74HC00N::SN74HC00N() {
  _initialized = false;
  _lastSwitches = 0;
  _lastOutputs = {0, 0, 0, 0};
}



uint8_t SN74HC00N::nandGate(uint8_t a, uint8_t b) {


  if (a && b) {
    return HC00N_HIGH; // 1 AND 1 = 1
  } else {
    return HC00N_LOW;
  }
}



uint8_t SN74HC00N::calculateNANDOutput(uint8_t switches) {

  uint8_t raw1A = (switches >> 0) & 0x01;
  uint8_t raw1B = (switches >> 1) & 0x01;
  uint8_t raw2A = (switches >> 2) & 0x01;
  uint8_t raw2B = (switches >> 3) & 0x01;
  uint8_t raw3A = (switches >> 4) & 0x01;
  uint8_t raw3B = (switches >> 5) & 0x01;
  uint8_t raw4A = (switches >> 6) & 0x01;
  uint8_t raw4B = (switches >> 7) & 0x01;





  uint8_t sw1A = raw1A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw1B = raw1B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw2A = raw2A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw2B = raw2B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw3A = raw3A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw3B = raw3B ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw4A = raw4A ? HC00N_LOW : HC00N_HIGH;
  uint8_t sw4B = raw4B ? HC00N_LOW : HC00N_HIGH;


  uint8_t out = 0;
  if (nandGate(sw1A, sw1B))
    out |= 0x01;
  if (nandGate(sw2A, sw2B))
    out |= 0x02; // bit1 = 2Y
  if (nandGate(sw3A, sw3B))
    out |= 0x04; // bit2 = 3Y
  if (nandGate(sw4A, sw4B))
    out |= 0x08; // bit3 = 4Y

  return out;
}



uint8_t SN74HC00N::readOutputByte(void) {
  if (!_initialized)
    return 0;

  uint8_t out = 0;

  if (HAL_GPIO_ReadPin(HC00N_OUT1_PORT, HC00N_OUT1_PIN) == GPIO_PIN_SET) {
    out |= 0x01;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT2_PORT, HC00N_OUT2_PIN) == GPIO_PIN_SET) {
    out |= 0x02;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT3_PORT, HC00N_OUT3_PIN) == GPIO_PIN_SET) {
    out |= 0x04;
  }
  if (HAL_GPIO_ReadPin(HC00N_OUT4_PORT, HC00N_OUT4_PIN) == GPIO_PIN_SET) {
    out |= 0x08;
  }

  return out;
}


HC00N_Outputs_t SN74HC00N::readOutputs(void) {
  HC00N_Outputs_t outs;
  uint8_t data = readOutputByte();

  outs.output1 = (data >> 0) & 0x01;
  outs.output2 = (data >> 1) & 0x01;
  outs.output3 = (data >> 2) & 0x01;
  outs.output4 = (data >> 3) & 0x01;

  return outs;
}


uint8_t SN74HC00N::readOutput(uint8_t channel) {
  if (channel >= 4)
    return 0;

  switch (channel) {
  case 0:
    return HAL_GPIO_ReadPin(HC00N_OUT1_PORT, HC00N_OUT1_PIN) == GPIO_PIN_SET;
  case 1:
    return HAL_GPIO_ReadPin(HC00N_OUT2_PORT, HC00N_OUT2_PIN) == GPIO_PIN_SET;
  case 2:
    return HAL_GPIO_ReadPin(HC00N_OUT3_PORT, HC00N_OUT3_PIN) == GPIO_PIN_SET;
  case 3:
    return HAL_GPIO_ReadPin(HC00N_OUT4_PORT, HC00N_OUT4_PIN) == GPIO_PIN_SET;
  default:
    return 0;
  }
}


void SN74HC00N::init(void) {
  if (_initialized)
    return;

  _initialized = true;

  LOG_I("HC00", "SN74HC00N Driver Initialized");
  LOG_I("HC00", "Inputs: 8 switches (1A1B-4A4B)");
  LOG_I("HC00", "Outputs: PE10=1Y, PE11=2Y, PE12=3Y, PE13=4Y");
  LOG_I("HC00", "Logic: Y = NOT (A AND B)");
  LOG_I("HC00", "Note: Input logic flipped for your hardware");


  _lastOutputs = readOutputs();
}


void SN74HC00N::updateFromSwitches(uint8_t switchStates) {
  _lastSwitches = switchStates;


  uint8_t expected = calculateNANDOutput(switchStates);


  uint8_t actual = readOutputByte();

  _lastOutputs.output1 = (actual >> 0) & 0x01;
  _lastOutputs.output2 = (actual >> 1) & 0x01;
  _lastOutputs.output3 = (actual >> 2) & 0x01;
  _lastOutputs.output4 = (actual >> 3) & 0x01;


  if (expected != actual) {
    LOG_W("HC00", "Mismatch! Expected: 0x%02X, Actual: 0x%02X", expected,
           actual);
  }
}


HC00N_Switches_t SN74HC00N::getSwitchStates(void) {
  HC00N_Switches_t sw;

  sw.sw1A = (_lastSwitches >> 0) & 0x01;
  sw.sw1B = (_lastSwitches >> 1) & 0x01;
  sw.sw2A = (_lastSwitches >> 2) & 0x01;
  sw.sw2B = (_lastSwitches >> 3) & 0x01;
  sw.sw3A = (_lastSwitches >> 4) & 0x01;
  sw.sw3B = (_lastSwitches >> 5) & 0x01;
  sw.sw4A = (_lastSwitches >> 6) & 0x01;
  sw.sw4B = (_lastSwitches >> 7) & 0x01;

  return sw;
}


HC00N_Outputs_t SN74HC00N::getActualOutputs(void) { return _lastOutputs; }


bool SN74HC00N::verifyOutputs(uint8_t switches) {
  uint8_t expected = calculateNANDOutput(switches);
  uint8_t actual = readOutputByte();
  return expected == actual;
}


void SN74HC00N::debugPrint(void) {
  HC00N_Switches_t sw = getSwitchStates();
  HC00N_Outputs_t outs = getActualOutputs();
  uint8_t raw = readOutputByte();
  (void)sw; (void)outs; (void)raw;

  LOG_D("HC00", "========== HC00N Status ==========");
  LOG_D("HC00", "Input Switches (raw: 1=closed, 0=open):");
  LOG_D("HC00", "  CH1: A=%d, B=%d", sw.sw1A, sw.sw1B);
  LOG_D("HC00", "  CH2: A=%d, B=%d", sw.sw2A, sw.sw2B);
  LOG_D("HC00", "  CH3: A=%d, B=%d", sw.sw3A, sw.sw3B);
  LOG_D("HC00", "  CH4: A=%d, B=%d", sw.sw4A, sw.sw4B);
  LOG_D("HC00", "Outputs (read from PE10-PE13):");
  LOG_D("HC00", "  1Y=%d, 2Y=%d, 3Y=%d, 4Y=%d", outs.output1, outs.output2,
         outs.output3, outs.output4);
  LOG_D("HC00", "Raw output byte: 0x%02X", raw);
  LOG_D("HC00", "Logic: Y = NOT (A AND B)");
  LOG_D("HC00", "==================================");
}