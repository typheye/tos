/**
 ******************************************************************************
 * @file    jy901s.hpp
 * @author  Typheye
 * @brief   Jy901S interface.
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

#ifndef __JY901S_HPP
#define __JY901S_HPP

#include "main.h"
#include "stm32f4xx_hal.h"
#include <cstring>
#include <stdio.h>



#define JY901S_ADDR_7BIT 0x50   
#define JY901S_ADDR (0x50 << 1) 


#define JY901S_ACC_X 0x34   
#define JY901S_ACC_Y 0x36   
#define JY901S_ACC_Z 0x38   
#define JY901S_GYRO_X 0x3A  
#define JY901S_GYRO_Y 0x3C  
#define JY901S_GYRO_Z 0x3E  
#define JY901S_ANGLE_X 0x40 
#define JY901S_ANGLE_Y 0x42 
#define JY901S_ANGLE_Z 0x44 
#define JY901S_MAG_X 0x46   
#define JY901S_MAG_Y 0x48   
#define JY901S_MAG_Z 0x4A   
#define JY901S_TEMP 0x52    
#define JY901S_VERSION 0x5C 






#define ACC_SCALE 2048.0f
#define GYRO_SCALE 16.4f
#define ANGLE_SCALE 182.044f
#define TEMP_SCALE 100.0f


typedef struct {
  float acc_x;       
  float acc_y;       
  float acc_z;       
  float gyro_x;      
  float gyro_y;      
  float gyro_z;      
  float roll;        
  float pitch;       
  float yaw;         
  float mag_x;       
  float mag_y;       
  float mag_z;       
  float temperature; 
} JY901S_Data_t;


typedef struct {
  int16_t acc_x;
  int16_t acc_y;
  int16_t acc_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t roll;
  int16_t pitch;
  int16_t yaw;
  int16_t mag_x;
  int16_t mag_y;
  int16_t mag_z;
  int16_t temperature;
} JY901S_Raw_t;

class JY901S {
public:
  JY901S();

  void init(void);
  bool checkConnection(void);
  bool isInitialized(void) { return _initialized; }

  
  JY901S_Raw_t readRaw(void);

  
  JY901S_Data_t readData(void);

  
  void readAccel(float *x, float *y, float *z);
  void readGyro(float *x, float *y, float *z);
  void readAngle(float *roll, float *pitch, float *yaw);
  void readMag(float *x, float *y, float *z);
  float readTemp(void);
  uint8_t readVersion(void);

  
  int16_t readReg16(uint8_t reg);
  void readRegs(uint8_t reg, uint8_t *buffer, uint8_t len);

private:
  I2C_HandleTypeDef *_hi2c;
  uint16_t _addr; 
  bool _initialized;

  bool readReg(uint8_t reg, uint8_t *value);
  bool writeReg(uint8_t reg, uint8_t value);
};

extern JY901S boardJY901S;

#endif // __JY901S_HPP