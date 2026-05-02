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

#ifdef __cplusplus
}
#endif

// SD 卡状态枚举
typedef enum {
  SD_CARD_OK = 0,
  SD_CARD_ERROR = 1,
  SD_CARD_NOT_READY = 2,
  SD_CARD_NO_CARD = 3,
  SD_CARD_WRITE_PROTECT = 4
} SDCard_Status_t;

// SD 卡信息结构体
typedef struct {
  uint32_t block_size;  // 块大小 (通常512字节)
  uint32_t block_count; // 块数量
  uint32_t capacity_mb; // 容量 (MB)
  uint8_t card_type;    // 卡类型
  uint8_t bus_width;    // 总线宽度 (1/4 bit)
} SDCard_Info_t;

class TSDIO {
public:
  // 构造函数
  TSDIO();

  // 初始化 SD 卡
  SDCard_Status_t init(void);

  // 获取 SD 卡状态
  SDCard_Status_t getStatus(void);

  // 获取 SD 卡信息
  SDCard_Info_t getInfo(void);

  // 读取扇区 (512字节)
  SDCard_Status_t readSector(uint8_t *buffer, uint32_t sector);

  // 写入扇区 (512字节)
  SDCard_Status_t writeSector(uint8_t *buffer, uint32_t sector);

  // 读取多个扇区
  SDCard_Status_t readMultiSector(uint8_t *buffer, uint32_t sector,
                                  uint32_t count);

  // 写入多个扇区
  SDCard_Status_t writeMultiSector(uint8_t *buffer, uint32_t sector,
                                   uint32_t count);

  // 擦除块
  SDCard_Status_t eraseBlock(uint32_t start_sector, uint32_t end_sector);

  // 检查 SD 卡是否插入
  bool isInserted(void);

  // 检查 SD 卡是否写保护
  bool isWriteProtected(void);

  // 测试 SD 卡读写
  bool selfTest(void);

  bool directWriteTest(void);
  bool simpleWriteTest(void);

private:
  bool initialized;        // 初始化标志
  bool write_protected;    // 写保护标志
  SDCard_Info_t card_info; // 卡信息

  // 等待 SD 卡就绪
  bool waitForReady(uint32_t timeout_ms);

  // 更新卡信息
  void updateCardInfo(void);
};

// 全局实例
extern TSDIO boardSDIO;

#endif // __TSDIO_HPP