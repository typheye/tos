/**
 ******************************************************************************
 * @file    tsdio.cpp
 * @author  Typheye
 * @brief   Tsdio implementation.
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

#include "include/tsdio.hpp"
#include "library/include/libdly.h"
#include "core/sys/include/syswatchdog.h"

/* Keep boot-time SDIO conservative. Some cards hang inside the HAL 4-bit bus
 * switch long enough for IWDG to reset the board. Enable this only after
 * card init and REC init are proven stable on the target hardware. */
#ifndef TSDIO_ENABLE_WIDE_BUS
#define TSDIO_ENABLE_WIDE_BUS 0
#endif



extern SD_HandleTypeDef hsd;


TSDIO boardSDIO;

extern USART boardSerial;


TSDIO::TSDIO() {
  _initialized = false;
  _hardDisabled = false;
  _writeProtected = false;
  memset(&_cardInfo, 0, sizeof(_cardInfo));
}


bool TSDIO::waitForReady(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  while (HAL_GetTick() - start < timeout_ms) {
    HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd);
    if (card_state == HAL_SD_CARD_TRANSFER) {
      return true;
    }
    if (card_state == HAL_SD_CARD_ERROR) {
      return false;
    }
    SysWatchdog_Tick();
    JPDelay(1);
  }
  return false;
}


void TSDIO::updateCardInfo(void) {
  HAL_SD_CardInfoTypeDef hal_card_info;

  if (HAL_SD_GetCardInfo(&hsd, &hal_card_info) == HAL_OK) {
    _cardInfo.block_size = hal_card_info.BlockSize;
    _cardInfo.block_count = hal_card_info.BlockNbr;
    _cardInfo.capacity_mb = (uint32_t)(((uint64_t)hal_card_info.BlockNbr *
                                        hal_card_info.BlockSize) /
                                       (1024 * 1024));
    _cardInfo.card_type = hal_card_info.CardType;
    _cardInfo.bus_width = 4;
  }
}



SDCard_Status_t TSDIO::init(void) {
  HAL_SD_CardInfoTypeDef hal_card_info;

  memset(&hal_card_info, 0, sizeof(hal_card_info));
  SysWatchdog_FeedNow();

  LOG_I("SDIO", "Starting init...");


  LOG_I("SDIO", "Calling HAL_SD_Init...");
  if (HAL_SD_Init(&hsd) != HAL_OK) {
    LOG_E("SDIO", "HAL_SD_Init FAILED");
    _hardDisabled = true;
    LOG_E("SDIO", "SD card HARD DISABLED â€?init failed");
    return SD_CARD_ERROR;
  }
  LOG_I("SDIO", "HAL_SD_Init OK");


  SysWatchdog_FeedNow();
  JPDelay(200);
  SysWatchdog_FeedNow();


  LOG_I("SDIO", "Checking card presence...");
  LOG_I("SDIO", "Card state after init: %ld", (long)HAL_SD_GetCardState(&hsd));


  LOG_I("SDIO", "Getting card info...");


  int retry = 5;
  while (retry--) {
    if (HAL_SD_GetCardInfo(&hsd, &hal_card_info) == HAL_OK) {
      if (hal_card_info.BlockNbr > 0) {
        break;
      }
    }
    LOG_W("SDIO", "Retrying get card info...");
    SysWatchdog_FeedNow();
    JPDelay(100);
  }

  if (hal_card_info.BlockNbr == 0) {
    LOG_I("SDIO", "Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu",
            (unsigned long)hal_card_info.CardType, (unsigned long)hal_card_info.BlockSize,
            (unsigned long)hal_card_info.BlockNbr);
    LOG_E("SDIO", "Failed to get valid card info!");
    _hardDisabled = true;
    LOG_E("SDIO", "SD card HARD DISABLED â€?invalid card info");
    return SD_CARD_ERROR;
  }

  LOG_I("SDIO", "Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu",
          (unsigned long)hal_card_info.CardType, (unsigned long)hal_card_info.BlockSize,
          (unsigned long)hal_card_info.BlockNbr);


  LOG_I("SDIO", "Configuring bus width...");


  _cardInfo.bus_width = 1;


#if TSDIO_ENABLE_WIDE_BUS && defined(SDIO_BUS_WIDE_4B)
  SysWatchdog_FeedNow();
  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) == HAL_OK) {
    _cardInfo.bus_width = 4;
    LOG_I("SDIO", "4-bit mode enabled");
  } else {
    LOG_W("SDIO", "4-bit mode failed, using 1-bit");
  }
  SysWatchdog_FeedNow();
#else
  LOG_I("SDIO", "4-bit mode disabled at boot, using 1-bit");
#endif


  _cardInfo.block_size = hal_card_info.BlockSize;
  _cardInfo.block_count = hal_card_info.BlockNbr;
  _cardInfo.capacity_mb =
      (uint32_t)(((uint64_t)hal_card_info.BlockNbr * hal_card_info.BlockSize) /
                 (1024 * 1024));
  _cardInfo.card_type = hal_card_info.CardType;


  SysWatchdog_FeedNow();
  LOG_I("SDIO", "Waiting for card ready...");
  if (!waitForReady(5000)) {
    LOG_E("SDIO", "Card ready timeout");
    _hardDisabled = true;
    LOG_E("SDIO", "SD card HARD DISABLED â€?not ready");
    return SD_CARD_NOT_READY;
  }

  _initialized = true;
  _hardDisabled = false;
  LOG_I("SDIO", "Init complete!");
  return SD_CARD_OK;
}


SDCard_Status_t TSDIO::getStatus(void) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  HAL_SD_CardStateTypeDef card_state = HAL_SD_GetCardState(&hsd);

  if (card_state == HAL_SD_CARD_TRANSFER) {
    return SD_CARD_OK;
  } else if (card_state == HAL_SD_CARD_ERROR) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}


SDCard_Info_t TSDIO::getInfo(void) {
  if (!_initialized) {
    updateCardInfo();
  }
  return _cardInfo;
}


SDCard_Status_t TSDIO::readSector(uint8_t *buffer, uint32_t sector) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  if (HAL_SD_ReadBlocks(&hsd, buffer, sector, 1, HAL_MAX_DELAY) != HAL_OK) {
    return SD_CARD_ERROR;
  }

  if (!waitForReady(5000)) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}



SDCard_Status_t TSDIO::writeSector(uint8_t *buffer, uint32_t sector) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  if (_writeProtected) {
    return SD_CARD_WRITE_PROTECT;
  }





  HAL_SD_CardInfoTypeDef card_info;
  HAL_SD_GetCardInfo(&hsd, &card_info);

  LOG_I("SDIO", "Write Card Type: %lu, Sector: %lu",
          (unsigned long)card_info.CardType, (unsigned long)sector);


  if (card_info.CardType == 1) {
    LOG_I("SDIO", "Write SDHC/SDXC card detected");
  }

  if (HAL_SD_WriteBlocks(&hsd, buffer, sector, 1, HAL_MAX_DELAY) != HAL_OK) {
    uint32_t error = HAL_SD_GetError(&hsd);
    LOG_E("SDIO", "Write Error code: 0x%08lX", (unsigned long)error);
    return SD_CARD_ERROR;
  }

  if (!waitForReady(5000)) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}


SDCard_Status_t TSDIO::readMultiSector(uint8_t *buffer, uint32_t sector,
                                       uint32_t count) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  if (HAL_SD_ReadBlocks(&hsd, buffer, sector, count, HAL_MAX_DELAY) != HAL_OK) {
    return SD_CARD_ERROR;
  }

  if (!waitForReady(5000)) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}


SDCard_Status_t TSDIO::writeMultiSector(uint8_t *buffer, uint32_t sector,
                                        uint32_t count) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  if (_writeProtected) {
    return SD_CARD_WRITE_PROTECT;
  }

  if (HAL_SD_WriteBlocks(&hsd, buffer, sector, count, HAL_MAX_DELAY) !=
      HAL_OK) {
    return SD_CARD_ERROR;
  }

  if (!waitForReady(5000)) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}


SDCard_Status_t TSDIO::eraseBlock(uint32_t start_sector, uint32_t end_sector) {
  if (!_initialized) {
    return SD_CARD_NOT_READY;
  }

  if (_writeProtected) {
    return SD_CARD_WRITE_PROTECT;
  }

  if (HAL_SD_Erase(&hsd, start_sector, end_sector) != HAL_OK) {
    return SD_CARD_ERROR;
  }

  if (!waitForReady(5000)) {
    return SD_CARD_ERROR;
  }

  return SD_CARD_OK;
}


bool TSDIO::isInserted(void) {
  if (_hardDisabled) return false;
  HAL_SD_CardInfoTypeDef card_info_test;
  return (HAL_SD_GetCardInfo(&hsd, &card_info_test) == HAL_OK);
}


bool TSDIO::isWriteProtected(void) { return _writeProtected; }


bool TSDIO::selfTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  if (!_initialized) {
    return false;
  }


  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)(i & 0xFF);
  }

  uint32_t test_sector = 0;

  if (_cardInfo.block_count > 100) {
    test_sector = _cardInfo.block_count - 1;
  } else {
    test_sector = 10;
  }


  uint8_t backup_buf[512];
  readSector(backup_buf, test_sector);


  if (writeSector(write_buf, test_sector) != SD_CARD_OK) {
    writeSector(backup_buf, test_sector);
    return false;
  }

  JPDelay(10);


  if (readSector(read_buf, test_sector) != SD_CARD_OK) {
    writeSector(backup_buf, test_sector);
    return false;
  }


  bool test_passed = (memcmp(write_buf, read_buf, 512) == 0);


  writeSector(backup_buf, test_sector);

  return test_passed;
}


bool TSDIO::directWriteTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];


  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    LOG_E("SDIO", "Direct Test: Cannot get card info");
    return false;
  }

  LOG_I("SDIO", "Direct Test: Card Type: %lu, BlockSize: %lu, BlockNbr: %lu",
          (unsigned long)card_info.CardType, (unsigned long)card_info.BlockSize,
          (unsigned long)card_info.BlockNbr);


  uint32_t test_sector = 1000;
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr - 100;
  }
  LOG_I("SDIO", "Direct Test: Using test sector: %lu", (unsigned long)test_sector);


  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)(i % 256);
  }


  LOG_I("SDIO", "Direct Test: Checking card state...");
  HAL_SD_CardStateTypeDef state;
  for (int i = 0; i < 1000; i++) {
    state = HAL_SD_GetCardState(&hsd);
    if (state == HAL_SD_CARD_READY || state == HAL_SD_CARD_TRANSFER) {
      break;
    }
    SysWatchdog_Tick();
    JPDelay(1);
  }
  LOG_I("SDIO", "Direct Test: Card state: %ld", (long)state);

  LOG_I("SDIO", "Direct Test: Writing sector...");


  HAL_StatusTypeDef result =
      HAL_SD_WriteBlocks(&hsd, write_buf, test_sector, 1, HAL_MAX_DELAY);
  LOG_I("SDIO", "Direct Test: HAL_SD_WriteBlocks result: %d", result);

  if (result != HAL_OK) {
    uint32_t error_code = HAL_SD_GetError(&hsd);
    LOG_E("SDIO", "Direct Test: Error code: 0x%08lX", (unsigned long)error_code);


    LOG_D("SDIO", "Direct Test: Error flags -- checking...");
    if (error_code & HAL_SD_ERROR_NONE)
      LOG_D("SDIO", "  NONE");
    if (error_code & HAL_SD_ERROR_CMD_CRC_FAIL)
      LOG_D("SDIO", "  CMD_CRC_FAIL");
    if (error_code & HAL_SD_ERROR_DATA_CRC_FAIL)
      LOG_D("SDIO", "  DATA_CRC_FAIL");
    if (error_code & HAL_SD_ERROR_CMD_RSP_TIMEOUT)
      LOG_D("SDIO", "  CMD_RSP_TIMEOUT");
    if (error_code & HAL_SD_ERROR_DATA_TIMEOUT)
      LOG_D("SDIO", "  DATA_TIMEOUT");
    if (error_code & HAL_SD_ERROR_TX_UNDERRUN)
      LOG_D("SDIO", "  TX_UNDERRUN");
    if (error_code & HAL_SD_ERROR_RX_OVERRUN)
      LOG_D("SDIO", "  RX_OVERRUN");
    if (error_code & HAL_SD_ERROR_ADDR_MISALIGNED)
      LOG_D("SDIO", "  ADDR_MISALIGNED");
    if (error_code & HAL_SD_ERROR_BLOCK_LEN_ERR)
      LOG_D("SDIO", "  BLOCK_LEN_ERR");
    if (error_code & HAL_SD_ERROR_ERASE_SEQ_ERR)
      LOG_D("SDIO", "  ERASE_SEQ_ERR");
    if (error_code & HAL_SD_ERROR_BAD_ERASE_PARAM)
      LOG_D("SDIO", "  BAD_ERASE_PARAM");
    if (error_code & HAL_SD_ERROR_WRITE_PROT_VIOLATION)
      LOG_D("SDIO", "  WRITE_PROT_VIOLATION");
    if (error_code & HAL_SD_ERROR_LOCK_UNLOCK_FAILED)
      LOG_D("SDIO", "  LOCK_UNLOCK_FAILED");
    if (error_code & HAL_SD_ERROR_COM_CRC_FAILED)
      LOG_D("SDIO", "  COM_CRC_FAILED");
    if (error_code & HAL_SD_ERROR_DMA)
      LOG_D("SDIO", "  DMA");
    if (error_code & HAL_SD_ERROR_UNSUPPORTED_FEATURE)
      LOG_D("SDIO", "  UNSUPPORTED_FEATURE");

    return false;
  }

  LOG_I("SDIO", "Direct Test: Write OK, waiting for completion...");


  if (!waitForReady(5000)) {
    LOG_E("SDIO", "Direct Test: Wait timeout");
    return false;
  }

  LOG_I("SDIO", "Direct Test: Reading back...");
  JPDelay(50);

  result = HAL_SD_ReadBlocks(&hsd, read_buf, test_sector, 1, HAL_MAX_DELAY);
  LOG_I("SDIO", "Direct Test: HAL_SD_ReadBlocks result: %d", result);

  if (result != HAL_OK) {
    LOG_E("SDIO", "Direct Test: Read failed");
    return false;
  }

  if (!waitForReady(5000)) {
    LOG_E("SDIO", "Direct Test: Read wait timeout");
    return false;
  }


  {
    char hex[128];
    int pos = 0;
    for (int i = 0; i < 32 && pos < (int)sizeof(hex) - 4; i++) {
      pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", read_buf[i]);
    }
    LOG_D("SDIO", "Direct Test: first 32 bytes: %s", hex);
  }


  if (memcmp(write_buf, read_buf, 512) == 0) {
    LOG_I("SDIO", "Direct Test: PASSED!");
    return true;
  } else {
    LOG_E("SDIO", "Direct Test: Data mismatch! FAILED!");
    return false;
  }
}


bool TSDIO::simpleWriteTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  LOG_I("SDIO", "Simple Test: Starting...");


  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    LOG_E("SDIO", "Simple Test: Cannot get card info");
    return false;
  }


  uint32_t test_sector = 100;
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr / 2;
  }
  LOG_I("SDIO", "Simple Test: Using sector %lu", (unsigned long)test_sector);


  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)((i + HAL_GetTick()) & 0xFF);
  }


  if (writeSector(write_buf, test_sector) != SD_CARD_OK) {
    LOG_E("SDIO", "Simple Test: Write failed");
    return false;
  }

  JPDelay(10);


  if (readSector(read_buf, test_sector) != SD_CARD_OK) {
    LOG_E("SDIO", "Simple Test: Read failed");
    return false;
  }


  if (memcmp(write_buf, read_buf, 512) == 0) {
    LOG_I("SDIO", "Simple Test: PASSED");
    return true;
  } else {
    LOG_E("SDIO", "Simple Test: Data mismatch");
    return false;
  }
}



bool TSDIO_IsHardDisabled(void) { return boardSDIO.isHardDisabled(); }
bool TSDIO_IsInitialized(void) { return boardSDIO.isInitialized(); }
void TSDIO_MarkHardDisabled(void) { boardSDIO.markHardDisabled(); }
