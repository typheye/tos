/**
 ******************************************************************************
 * @file    tsdio.hpp
 * @author  Typheye
 * @brief   Tsdio interface.
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

#ifndef TSDIO_HPP
#define TSDIO_HPP

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
bool TSDIO_IsInitialized(void);
void TSDIO_MarkHardDisabled(void);

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


  bool isHardDisabled(void) { return _hardDisabled; }
  bool isInitialized(void) const { return _initialized && !_hardDisabled; }
  void markHardDisabled(void) { _initialized = false; _hardDisabled = true; }


  bool isInserted(void);


  bool isWriteProtected(void);


  bool selfTest(void);

  bool directWriteTest(void);
  bool simpleWriteTest(void);

private:
  bool _initialized;
  bool _hardDisabled;
  bool _writeProtected;
  SDCard_Info_t _cardInfo;


  bool waitForReady(uint32_t timeout_ms);


  void updateCardInfo(void);
};


extern TSDIO boardSDIO;

#endif // TSDIO_HPP
