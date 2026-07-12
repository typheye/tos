/**
 ******************************************************************************
 * @file    jy901s.cpp
 * @author  Typheye
 * @brief   Jy901S implementation.
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

#include "include/jy901s.hpp"
#include "library/include/libdly.h"



extern I2C_HandleTypeDef hi2c1;


JY901S boardJY901S;


JY901S::JY901S() {
  _hi2c = &hi2c1;
  _addr = JY901S_ADDR;
  _initialized = false;
}


void JY901S::init(void) {
  if (_initialized)
    return;

  JPDelay(200);

  if (checkConnection()) {
    _initialized = true;
  }
}


bool JY901S::checkConnection(void) {
  uint8_t test = 0;

  if (HAL_I2C_Mem_Read(_hi2c, _addr, JY901S_VERSION, I2C_MEMADD_SIZE_8BIT,
                       &test, 1, 100) == HAL_OK) {
    return true;
  }
  return false;
}


int16_t JY901S::readReg16(uint8_t reg) {
  uint8_t buffer[2];

  if (HAL_I2C_Mem_Read(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, buffer, 2,
                       100) != HAL_OK) {
    return 0;
  }

  return (int16_t)(buffer[0] | (buffer[1] << 8));
}


void JY901S::readRegs(uint8_t reg, uint8_t *buffer, uint8_t len) {
  HAL_I2C_Mem_Read(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, buffer, len, 100);
}


bool JY901S::readReg(uint8_t reg, uint8_t *value) {
  return HAL_I2C_Mem_Read(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, value, 1,
                          100) == HAL_OK;
}


bool JY901S::writeReg(uint8_t reg, uint8_t value) {
  return HAL_I2C_Mem_Write(_hi2c, _addr, reg, I2C_MEMADD_SIZE_8BIT, &value, 1,
                           100) == HAL_OK;
}


JY901S_Raw_t JY901S::readRaw(void) {
  JY901S_Raw_t raw;
  uint8_t buffer[26];
  memset(&raw, 0, sizeof(raw));
  memset(buffer, 0, sizeof(buffer));

  if (HAL_I2C_Mem_Read(_hi2c, _addr, JY901S_ACC_X, I2C_MEMADD_SIZE_8BIT, buffer,
                       26, 100) == HAL_OK) {
    raw.acc_x = (int16_t)(buffer[0] | (buffer[1] << 8));
    raw.acc_y = (int16_t)(buffer[2] | (buffer[3] << 8));
    raw.acc_z = (int16_t)(buffer[4] | (buffer[5] << 8));
    raw.gyro_x = (int16_t)(buffer[6] | (buffer[7] << 8));
    raw.gyro_y = (int16_t)(buffer[8] | (buffer[9] << 8));
    raw.gyro_z = (int16_t)(buffer[10] | (buffer[11] << 8));
    raw.roll = (int16_t)(buffer[12] | (buffer[13] << 8));
    raw.pitch = (int16_t)(buffer[14] | (buffer[15] << 8));
    raw.yaw = (int16_t)(buffer[16] | (buffer[17] << 8));
    raw.mag_x = (int16_t)(buffer[18] | (buffer[19] << 8));
    raw.mag_y = (int16_t)(buffer[20] | (buffer[21] << 8));
    raw.mag_z = (int16_t)(buffer[22] | (buffer[23] << 8));
    raw.temperature = (int16_t)(buffer[24] | (buffer[25] << 8));
  }

  return raw;
}


JY901S_Data_t JY901S::readData(void) {
  JY901S_Raw_t raw = readRaw();
  JY901S_Data_t data;


  data.acc_x = raw.acc_x / 100.0f;
  data.acc_y = raw.acc_y / 100.0f;
  data.acc_z = raw.acc_z / 100.0f;

  data.gyro_x = raw.gyro_x / 100.0f;
  data.gyro_y = raw.gyro_y / 100.0f;
  data.gyro_z = raw.gyro_z / 100.0f;

  data.roll = raw.roll / 100.0f;
  data.pitch = raw.pitch / 100.0f;
  data.yaw = raw.yaw / 100.0f;

  data.mag_x = raw.mag_x;
  data.mag_y = raw.mag_y;
  data.mag_z = raw.mag_z;


  data.temperature = raw.temperature / 100.0f;

  return data;
}


void JY901S::readAccel(float *x, float *y, float *z) {
  JY901S_Data_t data = readData();
  if (x)
    *x = data.acc_x;
  if (y)
    *y = data.acc_y;
  if (z)
    *z = data.acc_z;
}


void JY901S::readGyro(float *x, float *y, float *z) {
  JY901S_Data_t data = readData();
  if (x)
    *x = data.gyro_x;
  if (y)
    *y = data.gyro_y;
  if (z)
    *z = data.gyro_z;
}


void JY901S::readAngle(float *roll, float *pitch, float *yaw) {
  JY901S_Data_t data = readData();
  if (roll)
    *roll = data.roll;
  if (pitch)
    *pitch = data.pitch;
  if (yaw)
    *yaw = data.yaw;
}


void JY901S::readMag(float *x, float *y, float *z) {
  JY901S_Data_t data = readData();
  if (x)
    *x = data.mag_x;
  if (y)
    *y = data.mag_y;
  if (z)
    *z = data.mag_z;
}


float JY901S::readTemp(void) {
  int16_t temp_raw = readReg16(JY901S_TEMP);
  return temp_raw / 100.0f;
}


uint8_t JY901S::readVersion(void) {
  uint8_t version;
  if (readReg(JY901S_VERSION, &version)) {
    return version;
  }
  return 0;
}