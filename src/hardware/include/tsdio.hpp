/**
 ******************************************************************************
 * @file    tsdio.hpp
 * @author  Typheye
 * @brief   Tsdio interface.
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

#ifndef __TSDIO_HPP
#define __TSDIO_HPP

#include "fatfs.h"
#include "hardware/include/usart.hpp"
#include "main.h"
#include <cstdio>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
#include "bsp_driver_sd.h"
#include "sdio.h"
#include "stm32f4xx_hal_sd.h"
#include "core/sys/include/syslog.h"


bool TSDIO_IsHardDisabled(void);

#ifdef __cplusplus
}
#endif


typedef enum {
  SD_CARD_OK = 0,
  SD_CARD_ERROR = 1,
  SD_CARD_NOT_READY = 2,
  SD_CARD_NO_CARD = 3,
  SD_CARD_WRITE_PROTECT = 4
} SDCard_Status_t;


typedef struct {
  uint32_t block_size;  
  uint32_t block_count; 
  uint32_t capacity_mb; 
  uint8_t card_type;    
  uint8_t bus_width;    
} SDCard_Info_t;

class TSDIO {
public:
  
  TSDIO();

  
  SDCard_Status_t init(void);

  
  SDCard_Status_t getStatus(void);

  
  SDCard_Info_t getInfo(void);

  
  SDCard_Status_t readSector(uint8_t *buffer, uint32_t sector);

  
  SDCard_Status_t writeSector(uint8_t *buffer, uint32_t sector);

  
  SDCard_Status_t readMultiSector(uint8_t *buffer, uint32_t sector,
                                  uint32_t count);

  
  SDCard_Status_t writeMultiSector(uint8_t *buffer, uint32_t sector,
                                   uint32_t count);

  
  SDCard_Status_t eraseBlock(uint32_t start_sector, uint32_t end_sector);

  
  bool isHardDisabled(void) { return _hard_disabled; }

  
  bool isInserted(void);

  
  bool isWriteProtected(void);

  
  bool selfTest(void);

  bool directWriteTest(void);
  bool simpleWriteTest(void);

private:
  bool initialized;        
  bool _hard_disabled;     
  bool write_protected;    
  SDCard_Info_t card_info; 

  
  bool waitForReady(uint32_t timeout_ms);

  
  void updateCardInfo(void);
};


extern TSDIO boardSDIO;

#endif // __TSDIO_HPP