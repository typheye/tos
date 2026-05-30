/**
 * @file    sfhd.c
 * @brief   STM32F407 Flash 数据存储实现
 * @note    Flash 擦写寿命约 1 万次，频繁写入请使用滚存接口
 *          擦写操作会短暂阻塞中断（~50ms 擦除 + 编程时间）
 */
#include "include/sfhd.h"
#include "syslog.h"
#include <stdio.h>
#include <string.h>

/* ========== 内部宏 ========== */
#define FLASH_TIMEOUT    500u   /* 操作超时(ms) */
#define ALIGN4(x)        (((uint32_t)(x) + 3u) & ~3u)

/* ========== 内部: 等待 Flash 就绪 ========== */
static __attribute__((unused)) Flash_Status_t wait_ready(uint32_t timeout) {
  uint32_t tick = HAL_GetTick();
  while (HAL_FLASH_GetError() != 0) {
    if (HAL_GetTick() - tick > timeout) return FLASH_ERR_TIMEOUT;
  }
  return FLASH_OK;
}

/* ========== 内部: 解锁/上锁 ========== */
static inline void flash_unlock(void) { HAL_FLASH_Unlock(); }
static inline void flash_lock(void)   { HAL_FLASH_Lock(); }

/* ========== 擦除单个扇区 ========== */
static Flash_Status_t erase_sector(uint32_t sector, uint32_t addr) {
  flash_unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

  FLASH_EraseInitTypeDef cfg;
  cfg.TypeErase    = FLASH_TYPEERASE_SECTORS;
  cfg.Banks        = FLASH_BANK_1;
  cfg.Sector       = sector;
  cfg.NbSectors    = 1;
  cfg.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  uint32_t err;
  HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&cfg, &err);
  flash_lock();
  return (st == HAL_OK) ? FLASH_OK : FLASH_ERR_ERASE;
}

/* ========== 内部: 编程一个字(32bit) ========== */
static Flash_Status_t program_word(uint32_t addr, uint32_t data) {
  flash_unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
  Flash_Status_t ret = (st == HAL_OK) ? FLASH_OK : FLASH_ERR_PROGRAM;
  flash_lock();
  return ret;
}

/* ========== 内部: 编程多字 ========== */
static Flash_Status_t program_words(uint32_t addr, const uint32_t *data, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    Flash_Status_t st = program_word(addr + i * 4, data[i]);
    if (st != FLASH_OK) return st;
  }
  return FLASH_OK;
}

/* ========== CRC32 (标准多项式 0x04C11DB7) ========== */
uint32_t Flash_CRC32(const uint32_t *pData, uint32_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  const uint8_t *p = (const uint8_t *)pData;
  for (uint32_t i = 0; i < size; i++) {
    crc ^= p[i];
    for (int b = 0; b < 8; b++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
  }
  return ~crc;
}

/* ========== 内部: 校验 4 字节对齐 ========== */
static Flash_Status_t check_align(const uint32_t *pData, uint32_t size) {
  if (((uint32_t)pData & 0x3) != 0) return FLASH_ERR_ALIGN;
  if ((size & 0x3) != 0) return FLASH_ERR_ALIGN;
  if (size > FLASH_RECORD_MAX) return FLASH_ERR_SIZE;
  return FLASH_OK;
}

/* ==================================================================
 * 公共 API
 * ================================================================== */

/* ---------- 擦除数据扇区 ---------- */
Flash_Status_t Flash_Erase_Sector(void) {
  Flash_Status_t st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
  LOG_I("FLASH", "Erase sector %d: %s", FLASH_DATA_SECTOR,
        st == FLASH_OK ? "OK" : "FAIL");
  return st;
}

/* ---------- 写入（覆盖） ---------- */
Flash_Status_t Flash_Write(const uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  uint32_t record[FLASH_RECORD_MAX / 4 + 2];
  Flash_Record_Header_t hdr;
  hdr.magic    = FLASH_RECORD_MAGIC;
  hdr.datasize = dataSize;
  hdr.crc      = Flash_CRC32(pData, dataSize);

  memcpy(&record[0], &hdr, sizeof(hdr));
  /* 数据从 record[3] 开始，避免覆盖 record[2] 中的 datasize 字段 */
  memcpy(&record[3], pData, dataSize);
  uint32_t total = sizeof(hdr) + dataSize;
  uint32_t words = (total + 3) / 4;

  st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
  if (st != FLASH_OK) return st;

  st = program_words(FLASH_DATA_ADDR, record, words);
  LOG_I("FLASH", "Write %lu bytes: %s", (unsigned long)dataSize, st == FLASH_OK ? "OK" : "FAIL");
  return st;
}

/* ---------- 读取 ---------- */
Flash_Status_t Flash_Read(uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  Flash_Record_Header_t *phdr = (Flash_Record_Header_t *)FLASH_DATA_ADDR;
  if (phdr->magic != FLASH_RECORD_MAGIC) {
    LOG_W("FLASH", "No valid data found (bad magic)");
    return FLASH_ERR_CRC;
  }

  const uint32_t *src = (const uint32_t *)(FLASH_DATA_ADDR + sizeof(Flash_Record_Header_t));
  memcpy(pData, src, dataSize);

  uint32_t crc = Flash_CRC32(pData, dataSize);
  if (crc != phdr->crc) {
    LOG_E("FLASH", "CRC mismatch: stored=0x%08lX calc=0x%08lX", phdr->crc, crc);
    return FLASH_ERR_CRC;
  }
  LOG_I("FLASH", "Read %lu bytes OK", (unsigned long)dataSize);
  return FLASH_OK;
}

/* ---------- 带备份安全写入 ---------- */
Flash_Status_t Flash_Write_With_Backup(const uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  uint32_t total = sizeof(Flash_Record_Header_t) + dataSize;
  uint32_t words = (total + 3) / 4;
  uint32_t buf[FLASH_RECORD_MAX / 4 + 2];

  Flash_Record_Header_t hdr;
  hdr.magic    = FLASH_RECORD_MAGIC;
  hdr.crc      = Flash_CRC32(pData, dataSize);
  hdr.datasize = dataSize;
  memcpy(&buf[0], &hdr, sizeof(hdr));
  /* 数据从 buf[3] 开始，避免覆盖 buf[2] 中的 datasize 字段 */
  memcpy(&buf[3], pData, dataSize);

  /* Step 1: 将现有数据备份到备份区 */
  st = erase_sector(FLASH_BACKUP_SECTOR, FLASH_BACKUP_ADDR);
  if (st != FLASH_OK) return st;
  st = program_words(FLASH_BACKUP_ADDR, buf, words);
  if (st != FLASH_OK) return st;
  LOG_I("FLASH", "Backup saved (%lu bytes)", (unsigned long)total);

  /* Step 2: 擦除数据区并写入新数据 */
  st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
  if (st != FLASH_OK) return st;
  st = program_words(FLASH_DATA_ADDR, buf, words);
  if (st != FLASH_OK) {
    LOG_E("FLASH", "Write FAILED — restoring from backup");
    /* 从备份恢复 */
    for (uint32_t i = 0; i < words; i++)
      buf[i] = *(volatile uint32_t *)(FLASH_BACKUP_ADDR + i * 4);
    st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
    if (st == FLASH_OK)
      program_words(FLASH_DATA_ADDR, buf, words);
    return FLASH_ERR_PROGRAM;
  }

  /* Step 3: 清除备份（写零到魔术字即标记无效） */
  program_word(FLASH_BACKUP_ADDR, 0);
  LOG_I("FLASH", "Write with backup OK (%lu bytes)", (unsigned long)dataSize);
  return FLASH_OK;
}

/* ---------- 启动时检查并恢复备份 ---------- */
bool Flash_Check_Backup(void) {
  volatile uint32_t *magic = (volatile uint32_t *)FLASH_BACKUP_ADDR;
  if (*magic != FLASH_RECORD_MAGIC) return false;

  LOG_W("FLASH", "Found backup, restoring...");
  uint32_t buf[FLASH_RECORD_MAX / 4 + 2];
  uint32_t words = FLASH_RECORD_MAX / 4 + 2;
  for (uint32_t i = 0; i < words; i++)
    buf[i] = *(volatile uint32_t *)(FLASH_BACKUP_ADDR + i * 4);

  Flash_Status_t st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
  if (st == FLASH_OK) {
    program_words(FLASH_DATA_ADDR, buf, words);
    program_word(FLASH_BACKUP_ADDR, 0); /* 清除备份标记 */
    LOG_I("FLASH", "Backup restored OK");
  }
  return true;
}

/* ==================================================================
 * 滚存写入（延长 Flash 寿命）
 * 每条记录 = 8 字节头 + 用户数据，在扇区内顺序写入
 * 扇区写满后擦除并从头开始
 * ================================================================== */

/* 内部: 查找最后一条有效记录的位置（通过 datasize 字段步进） */
static uint32_t rolling_find_last(void) {
  uint32_t last = FLASH_DATA_ADDR;
  uint32_t addr = FLASH_DATA_ADDR;

  while (addr < FLASH_DATA_ADDR + FLASH_DATA_SIZE - FLASH_HDR_SIZE) {
    Flash_Record_Header_t *hdr = (Flash_Record_Header_t *)addr;
    if (hdr->magic != FLASH_RECORD_MAGIC) break;
    last = addr;
    uint32_t step = ALIGN4(FLASH_HDR_SIZE + hdr->datasize);
    if (step < FLASH_HDR_SIZE + 4) break; /* safety */
    addr += step;
  }
  return last;
}

Flash_Status_t Flash_Rolling_Write(const uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  uint32_t total = sizeof(Flash_Record_Header_t) + dataSize;
  uint32_t slot_size = ALIGN4(total);
  if (slot_size > FLASH_RECORD_MAX + sizeof(Flash_Record_Header_t)) return FLASH_ERR_SIZE;

  uint32_t buf[FLASH_RECORD_MAX / 4 + 2];
  Flash_Record_Header_t hdr;
  hdr.magic    = FLASH_RECORD_MAGIC;
  hdr.crc      = Flash_CRC32(pData, dataSize);
  hdr.datasize = dataSize;
  memcpy(&buf[0], &hdr, sizeof(hdr));
  /* 关键: 数据从 buf[3] 开始，避免覆盖 buf[2] 中的 datasize 字段！ */
  memcpy(&buf[3], pData, dataSize);
  uint32_t words = (total + 3) / 4;

  /* 找到写入位置 */
  uint32_t last = rolling_find_last();
  uint32_t next = last + slot_size;
  if (last == FLASH_DATA_ADDR && ((Flash_Record_Header_t *)FLASH_DATA_ADDR)->magic != FLASH_RECORD_MAGIC)
    next = FLASH_DATA_ADDR; /* 首次写入 */

  /* 扇区写满则擦除重来 */
  if (next + slot_size > FLASH_DATA_ADDR + FLASH_DATA_SIZE) {
    LOG_W("FLASH", "Rolling sector full, erasing...");
    st = erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
    if (st != FLASH_OK) return st;
    next = FLASH_DATA_ADDR;
  }

  st = program_words(next, buf, words);
  LOG_I("FLASH", "Rolling write @0x%08lX %lu bytes: %s",
        next, (unsigned long)dataSize, st == FLASH_OK ? "OK" : "FAIL");
  return st;
}

Flash_Status_t Flash_Rolling_Read(uint32_t *pData, uint32_t maxSize, uint32_t *outSize) {
  if (maxSize > FLASH_RECORD_MAX) maxSize = FLASH_RECORD_MAX;
  uint32_t last = rolling_find_last();
  Flash_Record_Header_t *hdr = (Flash_Record_Header_t *)last;

  if (hdr->magic != FLASH_RECORD_MAGIC) {
    LOG_D("FLASH", "No rolling data found");
    if (outSize) *outSize = 0;
    return FLASH_ERR_CRC;
  }

  /* 读取数据 — 使用 hdr->datasize（写入时保存的真实大小）进行 CRC 校验 */
  uint32_t storedSize = hdr->datasize;
  if (storedSize > FLASH_RECORD_MAX) {
    LOG_E("FLASH", "Corrupt record: datasize=%lu exceeds max", (unsigned long)storedSize);
    return FLASH_ERR_CRC;
  }
  uint32_t copySize = maxSize < storedSize ? maxSize : storedSize;
  const uint8_t *src = (const uint8_t *)(last + sizeof(Flash_Record_Header_t));
  memcpy(pData, src, copySize);

  uint32_t crc = Flash_CRC32(pData, storedSize);
  if (crc != hdr->crc) {
    LOG_E("FLASH", "Rolling CRC mismatch: stored=0x%08lX calc=0x%08lX",
          hdr->crc, crc);
    return FLASH_ERR_CRC;
  }
  if (outSize) *outSize = copySize;
  LOG_I("FLASH", "Rolling read @0x%08lX %lu bytes OK", last, (unsigned long)copySize);
  return FLASH_OK;
}

/* ---------- 调试打印 ---------- */
void Flash_Print_Data(const uint32_t *pData, uint32_t dataSize) {
  const uint8_t *p = (const uint8_t *)pData;
  char hex[160];
  int pos = 0;
  uint32_t limit = dataSize < 64 ? dataSize : 64;
  for (uint32_t i = 0; i < limit && pos < (int)sizeof(hex) - 4; i++)
    pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X ", p[i]);
  LOG_D("FLASH", "Data(%luB): %s%s", (unsigned long)dataSize, hex,
        dataSize > 64 ? "..." : "");
}
