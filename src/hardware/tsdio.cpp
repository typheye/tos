#include "include/tsdio.hpp"
#include "syslog.h"

// 外部 SDIO 句柄 (由 CubeMX 生成)
extern SD_HandleTypeDef hsd;

// 全局实例
TSDIO boardSDIO;

extern USART boardSerial;

// 构造函数
TSDIO::TSDIO() {
  initialized = false;
  _hard_disabled = false;
  write_protected = false;
  memset(&card_info, 0, sizeof(card_info));
}

// 等待 SD 卡就绪
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
    HAL_Delay(1);
  }
  return false;
}

// 更新卡信息
void TSDIO::updateCardInfo(void) {
  HAL_SD_CardInfoTypeDef hal_card_info;

  if (HAL_SD_GetCardInfo(&hsd, &hal_card_info) == HAL_OK) {
    card_info.block_size = hal_card_info.BlockSize;
    card_info.block_count = hal_card_info.BlockNbr;
    card_info.capacity_mb = (uint32_t)(((uint64_t)hal_card_info.BlockNbr *
                                        hal_card_info.BlockSize) /
                                       (1024 * 1024));
    card_info.card_type = hal_card_info.CardType;
    card_info.bus_width = 4;
  }
}

// 初始化 SD 卡
// 初始化 SD 卡
SDCard_Status_t TSDIO::init(void) {
  HAL_SD_CardInfoTypeDef hal_card_info;

  LOG_I("SDIO", "Starting init...");

  // 1. 初始化 SDIO 接口
  LOG_I("SDIO", "Calling HAL_SD_Init...");
  if (HAL_SD_Init(&hsd) != HAL_OK) {
    LOG_E("SDIO", "HAL_SD_Init FAILED");
    _hard_disabled = true;
    LOG_F("SDIO", "SD card HARD DISABLED — init failed");
    return SD_CARD_ERROR;
  }
  LOG_I("SDIO", "HAL_SD_Init OK");

  // 2. 重要：等待卡上电完成
  HAL_Delay(200);

  // 3. 检查卡是否插入 (通过检查卡状态)
  LOG_I("SDIO", "Checking card presence...");
  LOG_I("SDIO", "Card state after init: %ld", (long)HAL_SD_GetCardState(&hsd));

  // 4. 获取卡信息
  LOG_I("SDIO", "Getting card info...");

  // 尝试多次获取卡信息
  int retry = 5;
  while (retry--) {
    if (HAL_SD_GetCardInfo(&hsd, &hal_card_info) == HAL_OK) {
      if (hal_card_info.BlockNbr > 0) {
        break;
      }
    }
    LOG_W("SDIO", "Retrying get card info...");
    HAL_Delay(100);
  }

  if (hal_card_info.BlockNbr == 0) {
    LOG_I("SDIO", "Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu",
            (unsigned long)hal_card_info.CardType, (unsigned long)hal_card_info.BlockSize,
            (unsigned long)hal_card_info.BlockNbr);
    LOG_E("SDIO", "Failed to get valid card info!");
    _hard_disabled = true;
    LOG_F("SDIO", "SD card HARD DISABLED — invalid card info");
    return SD_CARD_ERROR;
  }

  LOG_I("SDIO", "Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu",
          (unsigned long)hal_card_info.CardType, (unsigned long)hal_card_info.BlockSize,
          (unsigned long)hal_card_info.BlockNbr);

  // 5. 配置总线宽度 (先尝试 1-bit，成功后再试 4-bit)
  LOG_I("SDIO", "Configuring bus width...");

  // 先用 1-bit 模式验证读写是否正常
  card_info.bus_width = 1;

// 可选：尝试 4-bit 模式
#ifdef SDIO_BUS_WIDE_4B
  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) == HAL_OK) {
    card_info.bus_width = 4;
    LOG_I("SDIO", "4-bit mode enabled");
  } else {
    LOG_W("SDIO", "4-bit mode failed, using 1-bit");
  }
#endif

  // 6. 更新卡信息
  card_info.block_size = hal_card_info.BlockSize;
  card_info.block_count = hal_card_info.BlockNbr;
  card_info.capacity_mb =
      (uint32_t)(((uint64_t)hal_card_info.BlockNbr * hal_card_info.BlockSize) /
                 (1024 * 1024));
  card_info.card_type = hal_card_info.CardType;

  // 7. 等待卡就绪
  LOG_I("SDIO", "Waiting for card ready...");
  if (!waitForReady(5000)) {
    LOG_E("SDIO", "Card ready timeout");
    _hard_disabled = true;
    LOG_F("SDIO", "SD card HARD DISABLED — not ready");
    return SD_CARD_NOT_READY;
  }

  initialized = true;
  _hard_disabled = false;
  LOG_I("SDIO", "Init complete!");
  return SD_CARD_OK;
}

// 获取 SD 卡状态
SDCard_Status_t TSDIO::getStatus(void) {
  if (!initialized) {
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

// 获取 SD 卡信息
SDCard_Info_t TSDIO::getInfo(void) {
  if (!initialized) {
    updateCardInfo();
  }
  return card_info;
}

// 读取单个扇区
SDCard_Status_t TSDIO::readSector(uint8_t *buffer, uint32_t sector) {
  if (!initialized) {
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

// 写入单个扇区
// 在 writeSector 函数开头添加
SDCard_Status_t TSDIO::writeSector(uint8_t *buffer, uint32_t sector) {
  if (!initialized) {
    return SD_CARD_NOT_READY;
  }

  if (write_protected) {
    return SD_CARD_WRITE_PROTECT;
  }

  // 对于 SDHC/SDXC 卡 (容量 > 2GB)，扇区地址需要使用块地址
  // HAL 库会自动处理，但这里做显式检查

  // 获取卡类型
  HAL_SD_CardInfoTypeDef card_info;
  HAL_SD_GetCardInfo(&hsd, &card_info);

  LOG_I("SDIO", "Write Card Type: %lu, Sector: %lu",
          (unsigned long)card_info.CardType, (unsigned long)sector);

  // 对于 SDHC/SDXC，CardType 是 1
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

// 读取多个扇区
SDCard_Status_t TSDIO::readMultiSector(uint8_t *buffer, uint32_t sector,
                                       uint32_t count) {
  if (!initialized) {
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

// 写入多个扇区
SDCard_Status_t TSDIO::writeMultiSector(uint8_t *buffer, uint32_t sector,
                                        uint32_t count) {
  if (!initialized) {
    return SD_CARD_NOT_READY;
  }

  if (write_protected) {
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

// 擦除块
SDCard_Status_t TSDIO::eraseBlock(uint32_t start_sector, uint32_t end_sector) {
  if (!initialized) {
    return SD_CARD_NOT_READY;
  }

  if (write_protected) {
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

// 检查 SD 卡是否插入
bool TSDIO::isInserted(void) {
  if (_hard_disabled) return false;
  HAL_SD_CardInfoTypeDef card_info_test;
  return (HAL_SD_GetCardInfo(&hsd, &card_info_test) == HAL_OK);
}

// 检查 SD 卡是否写保护
bool TSDIO::isWriteProtected(void) { return write_protected; }

// 自我测试
bool TSDIO::selfTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  if (!initialized) {
    return false;
  }

  // 准备测试数据
  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)(i & 0xFF);
  }

  uint32_t test_sector = 0;

  if (card_info.block_count > 100) {
    test_sector = card_info.block_count - 1;
  } else {
    test_sector = 10;
  }

  // 读取原始数据以便恢复
  uint8_t backup_buf[512];
  readSector(backup_buf, test_sector);

  // 写入测试数据
  if (writeSector(write_buf, test_sector) != SD_CARD_OK) {
    writeSector(backup_buf, test_sector);
    return false;
  }

  HAL_Delay(10);

  // 读取测试数据
  if (readSector(read_buf, test_sector) != SD_CARD_OK) {
    writeSector(backup_buf, test_sector);
    return false;
  }

  // 比较数据
  bool test_passed = (memcmp(write_buf, read_buf, 512) == 0);

  // 恢复原始数据
  writeSector(backup_buf, test_sector);

  return test_passed;
}

// 在 tsdio.cpp 中替换 directWriteTest 函数
bool TSDIO::directWriteTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  // 先获取卡信息
  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    LOG_E("SDIO", "Direct Test: Cannot get card info");
    return false;
  }

  LOG_I("SDIO", "Direct Test: Card Type: %lu, BlockSize: %lu, BlockNbr: %lu",
          (unsigned long)card_info.CardType, (unsigned long)card_info.BlockSize,
          (unsigned long)card_info.BlockNbr);

  // 使用一个安全的测试扇区
  uint32_t test_sector = 1000; // 改用扇区 1000
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr - 100;
  }
  LOG_I("SDIO", "Direct Test: Using test sector: %lu", (unsigned long)test_sector);

  // 准备数据
  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)(i % 256);
  }

  // 等待卡就绪
  LOG_I("SDIO", "Direct Test: Checking card state...");
  HAL_SD_CardStateTypeDef state;
  for (int i = 0; i < 1000; i++) {
    state = HAL_SD_GetCardState(&hsd);
    if (state == HAL_SD_CARD_READY || state == HAL_SD_CARD_TRANSFER) {
      break;
    }
    HAL_Delay(1);
  }
  LOG_I("SDIO", "Direct Test: Card state: %ld", (long)state);

  LOG_I("SDIO", "Direct Test: Writing sector...");

  // 写入
  HAL_StatusTypeDef result =
      HAL_SD_WriteBlocks(&hsd, write_buf, test_sector, 1, HAL_MAX_DELAY);
  LOG_I("SDIO", "Direct Test: HAL_SD_WriteBlocks result: %d", result);

  if (result != HAL_OK) {
    uint32_t error_code = HAL_SD_GetError(&hsd);
    LOG_E("SDIO", "Direct Test: Error code: 0x%08lX", (unsigned long)error_code);

    // 打印错误位（只打印存在的）
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

  // 等待写入完成
  if (!waitForReady(5000)) {
    LOG_E("SDIO", "Direct Test: Wait timeout");
    return false;
  }

  LOG_I("SDIO", "Direct Test: Reading back...");
  HAL_Delay(50);

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

  /* 显示前32字节 */
  {
    char hex[128];
    int pos = 0;
    for (int i = 0; i < 32 && pos < (int)sizeof(hex) - 4; i++) {
      pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", read_buf[i]);
    }
    LOG_D("SDIO", "Direct Test: first 32 bytes: %s", hex);
  }

  // 比较数据
  if (memcmp(write_buf, read_buf, 512) == 0) {
    LOG_I("SDIO", "Direct Test: PASSED!");
    return true;
  } else {
    LOG_E("SDIO", "Direct Test: Data mismatch! FAILED!");
    return false;
  }
}

// 在 tsdio.cpp 中添加
bool TSDIO::simpleWriteTest(void) {
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  LOG_I("SDIO", "Simple Test: Starting...");

  // 获取卡信息
  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    LOG_E("SDIO", "Simple Test: Cannot get card info");
    return false;
  }

  // 使用扇区 100（应该安全）
  uint32_t test_sector = 100;
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr / 2;
  }
  LOG_I("SDIO", "Simple Test: Using sector %lu", (unsigned long)test_sector);

  // 准备数据
  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)((i + HAL_GetTick()) & 0xFF);
  }

  // 写入
  if (writeSector(write_buf, test_sector) != SD_CARD_OK) {
    LOG_E("SDIO", "Simple Test: Write failed");
    return false;
  }

  HAL_Delay(10);

  // 读取
  if (readSector(read_buf, test_sector) != SD_CARD_OK) {
    LOG_E("SDIO", "Simple Test: Read failed");
    return false;
  }

  // 验证
  if (memcmp(write_buf, read_buf, 512) == 0) {
    LOG_I("SDIO", "Simple Test: PASSED");
    return true;
  } else {
    LOG_E("SDIO", "Simple Test: Data mismatch");
    return false;
  }
}

// ==================== C 接口 ====================

bool TSDIO_IsHardDisabled(void) { return boardSDIO.isHardDisabled(); }