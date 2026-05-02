#include "include/tsdio.hpp"

// 外部 SDIO 句柄 (由 CubeMX 生成)
extern SD_HandleTypeDef hsd;

// 全局实例
TSDIO boardSDIO;

extern USART boardSerial;

// 构造函数
TSDIO::TSDIO() {
  initialized = false;
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
  char dbg[64];
  ;

  printf("  [SDIO] Starting init...\r\n");

  // 1. 初始化 SDIO 接口
  printf("  [SDIO] Calling HAL_SD_Init...\r\n");
  if (HAL_SD_Init(&hsd) != HAL_OK) {
    printf("  [SDIO] HAL_SD_Init FAILED\r\n");
    return SD_CARD_ERROR;
  }
  printf("  [SDIO] HAL_SD_Init OK\r\n");

  // 2. 重要：等待卡上电完成
  HAL_Delay(200);

  // 3. 检查卡是否插入 (通过检查卡状态)
  printf("  [SDIO] Checking card presence...\r\n");
  HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd);
  sprintf(dbg, "  [SDIO] Card state after init: %ld\r\n", (long)state);
  printf(dbg);

  // 4. 获取卡信息
  printf("  [SDIO] Getting card info...\r\n");

  // 尝试多次获取卡信息
  int retry = 5;
  while (retry--) {
    if (HAL_SD_GetCardInfo(&hsd, &hal_card_info) == HAL_OK) {
      if (hal_card_info.BlockNbr > 0) {
        break;
      }
    }
    printf("  [SDIO] Retrying get card info...\r\n");
    HAL_Delay(100);
  }

  if (hal_card_info.BlockNbr == 0) {
    sprintf(dbg,
            "  [SDIO] Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu\r\n",
            hal_card_info.CardType, hal_card_info.BlockSize,
            hal_card_info.BlockNbr);
    printf(dbg);
    printf("  [SDIO] Failed to get valid card info!\r\n");
    return SD_CARD_ERROR;
  }

  sprintf(dbg, "  [SDIO] Card Info: Type=%lu, BlockSize=%lu, BlockNbr=%lu\r\n",
          hal_card_info.CardType, hal_card_info.BlockSize,
          hal_card_info.BlockNbr);
  printf(dbg);

  // 5. 配置总线宽度 (先尝试 1-bit，成功后再试 4-bit)
  printf("  [SDIO] Configuring bus width...\r\n");

  // 先用 1-bit 模式验证读写是否正常
  card_info.bus_width = 1;

// 可选：尝试 4-bit 模式
#ifdef SDIO_BUS_WIDE_4B
  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) == HAL_OK) {
    card_info.bus_width = 4;
    printf("  [SDIO] 4-bit mode enabled\r\n");
  } else {
    printf("  [SDIO] 4-bit mode failed, using 1-bit\r\n");
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
  printf("  [SDIO] Waiting for card ready...\r\n");
  if (!waitForReady(5000)) {
    printf("  [SDIO] Card ready timeout\r\n");
    return SD_CARD_NOT_READY;
  }

  initialized = true;
  printf("  [SDIO] Init complete!\r\n");
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

  char dbg[64];
  ;
  sprintf(dbg, "  [Write] Card Type: %lu, Sector: %lu\r\n", card_info.CardType,
          sector);
  printf(dbg);

  // 对于 SDHC/SDXC，CardType 是 1
  if (card_info.CardType == 1) {
    printf("  [Write] SDHC/SDXC card detected\r\n");
  }

  if (HAL_SD_WriteBlocks(&hsd, buffer, sector, 1, HAL_MAX_DELAY) != HAL_OK) {
    uint32_t error = HAL_SD_GetError(&hsd);
    sprintf(dbg, "  [Write] Error code: 0x%08lX\r\n", error);
    printf(dbg);
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
  char dbg[128];
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  // 先获取卡信息
  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    printf("  [Direct Test] Cannot get card info\r\n");
    return false;
  }

  sprintf(dbg,
          "  [Direct Test] Card Type: %lu, BlockSize: %lu, BlockNbr: %lu\r\n",
          card_info.CardType, card_info.BlockSize, card_info.BlockNbr);
  printf(dbg);

  // 使用一个安全的测试扇区
  uint32_t test_sector = 1000; // 改用扇区 1000
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr - 100;
  }
  sprintf(dbg, "  [Direct Test] Using test sector: %lu\r\n", test_sector);
  printf(dbg);

  // 准备数据
  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)(i % 256);
  }

  // 等待卡就绪
  printf("  [Direct Test] Checking card state...\r\n");
  HAL_SD_CardStateTypeDef state;
  for (int i = 0; i < 1000; i++) {
    state = HAL_SD_GetCardState(&hsd);
    if (state == HAL_SD_CARD_READY || state == HAL_SD_CARD_TRANSFER) {
      break;
    }
    HAL_Delay(1);
  }
  sprintf(dbg, "  [Direct Test] Card state: %ld\r\n", (long)state);
  printf(dbg);

  printf("  [Direct Test] Writing sector...\r\n");

  // 写入
  HAL_StatusTypeDef result =
      HAL_SD_WriteBlocks(&hsd, write_buf, test_sector, 1, HAL_MAX_DELAY);
  sprintf(dbg, "  [Direct Test] HAL_SD_WriteBlocks result: %d\r\n", result);
  printf(dbg);

  if (result != HAL_OK) {
    uint32_t error_code = HAL_SD_GetError(&hsd);
    sprintf(dbg, "  [Direct Test] Error code: 0x%08lX\r\n", error_code);
    printf(dbg);

    // 打印错误位（只打印存在的）
    printf("  [Direct Test] Error flags:\r\n");
    if (error_code & HAL_SD_ERROR_NONE)
      printf("    NONE\r\n");
    if (error_code & HAL_SD_ERROR_CMD_CRC_FAIL)
      printf("    CMD_CRC_FAIL\r\n");
    if (error_code & HAL_SD_ERROR_DATA_CRC_FAIL)
      printf("    DATA_CRC_FAIL\r\n");
    if (error_code & HAL_SD_ERROR_CMD_RSP_TIMEOUT)
      printf("    CMD_RSP_TIMEOUT\r\n");
    if (error_code & HAL_SD_ERROR_DATA_TIMEOUT)
      printf("    DATA_TIMEOUT\r\n");
    if (error_code & HAL_SD_ERROR_TX_UNDERRUN)
      printf("    TX_UNDERRUN\r\n");
    if (error_code & HAL_SD_ERROR_RX_OVERRUN)
      printf("    RX_OVERRUN\r\n");
    if (error_code & HAL_SD_ERROR_ADDR_MISALIGNED)
      printf("    ADDR_MISALIGNED\r\n");
    if (error_code & HAL_SD_ERROR_BLOCK_LEN_ERR)
      printf("    BLOCK_LEN_ERR\r\n");
    if (error_code & HAL_SD_ERROR_ERASE_SEQ_ERR)
      printf("    ERASE_SEQ_ERR\r\n");
    if (error_code & HAL_SD_ERROR_BAD_ERASE_PARAM)
      printf("    BAD_ERASE_PARAM\r\n");
    if (error_code & HAL_SD_ERROR_WRITE_PROT_VIOLATION)
      printf("    WRITE_PROT_VIOLATION\r\n");
    if (error_code & HAL_SD_ERROR_LOCK_UNLOCK_FAILED)
      printf("    LOCK_UNLOCK_FAILED\r\n");
    if (error_code & HAL_SD_ERROR_COM_CRC_FAILED)
      printf("    COM_CRC_FAILED\r\n");
    if (error_code & HAL_SD_ERROR_DMA)
      printf("    DMA\r\n");
    if (error_code & HAL_SD_ERROR_UNSUPPORTED_FEATURE)
      printf("    UNSUPPORTED_FEATURE\r\n");

    return false;
  }

  printf("  [Direct Test] Write OK, waiting for completion...\r\n");

  // 等待写入完成
  if (!waitForReady(5000)) {
    printf("  [Direct Test] Wait timeout\r\n");
    return false;
  }

  printf("  [Direct Test] Reading back...\r\n");
  HAL_Delay(50);

  result = HAL_SD_ReadBlocks(&hsd, read_buf, test_sector, 1, HAL_MAX_DELAY);
  sprintf(dbg, "  [Direct Test] HAL_SD_ReadBlocks result: %d\r\n", result);
  printf(dbg);

  if (result != HAL_OK) {
    printf("  [Direct Test] Read failed\r\n");
    return false;
  }

  if (!waitForReady(5000)) {
    printf("  [Direct Test] Read wait timeout\r\n");
    return false;
  }

  // 显示前32字节
  printf("  [Direct Test] First 32 bytes of read data:\r\n  ");
  for (int i = 0; i < 32; i++) {
    sprintf(dbg, "%02X ", read_buf[i]);
    printf(dbg);
    if ((i + 1) % 16 == 0)
      printf("\r\n  ");
  }
  printf("\r\n");

  // 比较数据
  if (memcmp(write_buf, read_buf, 512) == 0) {
    printf("  [Direct Test] PASSED!\r\n");
    return true;
  } else {
    printf("  [Direct Test] Data mismatch! FAILED!\r\n");
    return false;
  }
}

// 在 tsdio.cpp 中添加
bool TSDIO::simpleWriteTest(void) {
  char dbg[64];
  ;
  uint8_t write_buf[512];
  uint8_t read_buf[512];

  printf("  [Simple Test] Starting...\r\n");

  // 获取卡信息
  HAL_SD_CardInfoTypeDef card_info;
  if (HAL_SD_GetCardInfo(&hsd, &card_info) != HAL_OK) {
    printf("  [Simple Test] Cannot get card info\r\n");
    return false;
  }

  // 使用扇区 100（应该安全）
  uint32_t test_sector = 100;
  if (test_sector >= card_info.BlockNbr) {
    test_sector = card_info.BlockNbr / 2;
  }
  sprintf(dbg, "  [Simple Test] Using sector %lu\r\n", test_sector);
  printf(dbg);

  // 准备数据
  for (int i = 0; i < 512; i++) {
    write_buf[i] = (uint8_t)((i + HAL_GetTick()) & 0xFF);
  }

  // 写入
  if (writeSector(write_buf, test_sector) != SD_CARD_OK) {
    printf("  [Simple Test] Write failed\r\n");
    return false;
  }

  HAL_Delay(10);

  // 读取
  if (readSector(read_buf, test_sector) != SD_CARD_OK) {
    printf("  [Simple Test] Read failed\r\n");
    return false;
  }

  // 验证
  if (memcmp(write_buf, read_buf, 512) == 0) {
    printf("  [Simple Test] PASSED\r\n");
    return true;
  } else {
    printf("  [Simple Test] Data mismatch\r\n");
    return false;
  }
}