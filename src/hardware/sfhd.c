/**
 ******************************************************************************
 * @file    sfhd.c
 * @author  Typheye
 * @brief   Sfhd implementation.
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

#include "include/sfhd.h"



#define FLASH_TIMEOUT    500u   
#define ALIGN4(x)        (((uint32_t)(x) + 3u) & ~3u)
#define FLASH_BL_STATE_MAGIC   0x53424C55u
#define FLASH_BL_STATE_VERSION 2u
#define FLASH_BL_STATE_SECTOR  FLASH_SECTOR_3

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t version;
  uint32_t unlocked;
  uint32_t boot_target;
  uint32_t crc;
} Flash_BlStateRecord_t;


static __attribute__((unused)) Flash_Status_t wait_ready(uint32_t timeout) {
  uint32_t tick = HAL_GetTick();
  while (HAL_FLASH_GetError() != 0) {
    if (HAL_GetTick() - tick > timeout) return FLASH_ERR_TIMEOUT;
  }
  return FLASH_OK;
}


static inline void flash_unlock(void) { HAL_FLASH_Unlock(); }
static inline void flash_lock(void)   { HAL_FLASH_Lock(); }


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


static Flash_Status_t program_word(uint32_t addr, uint32_t data) {
  flash_unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
  Flash_Status_t ret = (st == HAL_OK) ? FLASH_OK : FLASH_ERR_PROGRAM;
  flash_lock();
  return ret;
}


static Flash_Status_t program_words(uint32_t addr, const uint32_t *data, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    Flash_Status_t st = program_word(addr + i * 4, data[i]);
    if (st != FLASH_OK) return st;
  }
  return FLASH_OK;
}

static uint32_t flash_bl_state_crc(const Flash_BlStateRecord_t *r) {
  return r->magic ^ r->version ^ r->unlocked ^ r->boot_target ^ 0xA5A55A5Au;
}

static bool flash_bl_state_valid(const Flash_BlStateRecord_t *r) {
  return r->magic == FLASH_BL_STATE_MAGIC &&
         r->version == FLASH_BL_STATE_VERSION &&
         r->crc == flash_bl_state_crc(r);
}

static bool flash_bl_state_erased(const Flash_BlStateRecord_t *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (w[i] != 0xFFFFFFFFu) {
      return false;
    }
  }
  return true;
}

static bool flash_bl_state_latest(Flash_BlStateRecord_t *out) {
  bool found = false;
  for (uint32_t off = 0; off + sizeof(Flash_BlStateRecord_t) <= FLASH_BL_STATE_SIZE;
       off += sizeof(Flash_BlStateRecord_t)) {
    const Flash_BlStateRecord_t *r =
        (const Flash_BlStateRecord_t *)(FLASH_BL_STATE_ADDR + off);
    if (flash_bl_state_erased(r)) {
      break;
    }
    if (flash_bl_state_valid(r)) {
      *out = *r;
      found = true;
    }
  }
  return found;
}

static Flash_Status_t flash_bl_state_append(uint32_t unlocked,
                                            uint32_t boot_target) {
  Flash_BlStateRecord_t r;
  uint32_t addr = 0;

  r.magic = FLASH_BL_STATE_MAGIC;
  r.version = FLASH_BL_STATE_VERSION;
  r.unlocked = unlocked ? 1u : 0u;
  r.boot_target = boot_target;
  r.crc = flash_bl_state_crc(&r);

  for (uint32_t off = 0; off + sizeof(Flash_BlStateRecord_t) <= FLASH_BL_STATE_SIZE;
       off += sizeof(Flash_BlStateRecord_t)) {
    const Flash_BlStateRecord_t *slot =
        (const Flash_BlStateRecord_t *)(FLASH_BL_STATE_ADDR + off);
    if (flash_bl_state_erased(slot)) {
      addr = FLASH_BL_STATE_ADDR + off;
      break;
    }
  }
  if (addr == 0) {
    Flash_Status_t est = erase_sector(FLASH_BL_STATE_SECTOR, FLASH_BL_STATE_ADDR);
    if (est != FLASH_OK) {
      return est;
    }
    addr = FLASH_BL_STATE_ADDR;
  }
  Flash_Status_t st = program_words(addr, (const uint32_t *)&r,
                                    sizeof(r) / sizeof(uint32_t));
  if (st == FLASH_OK) {
    return FLASH_OK;
  }

  st = erase_sector(FLASH_BL_STATE_SECTOR, FLASH_BL_STATE_ADDR);
  if (st != FLASH_OK) {
    return st;
  }
  return program_words(FLASH_BL_STATE_ADDR, (const uint32_t *)&r,
                       sizeof(r) / sizeof(uint32_t));
}

Flash_Status_t Flash_BL_SetBootTarget(uint32_t target) {
  Flash_BlStateRecord_t latest;
  uint32_t unlocked = 0;

  if (target != FLASH_BL_BOOT_FASTBOOT &&
      target != FLASH_BL_BOOT_RECOVERY &&
      target != FLASH_BL_BOOT_NONE) {
    return FLASH_ERR_SIZE;
  }
  if (flash_bl_state_latest(&latest)) {
    unlocked = latest.unlocked ? 1u : 0u;
  }
  return flash_bl_state_append(unlocked, target);
}

static Flash_Status_t erase_data_sector_preserve_bl(void) {
  return erase_sector(FLASH_DATA_SECTOR, FLASH_DATA_ADDR);
}


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


static Flash_Status_t check_align(const uint32_t *pData, uint32_t size) {
  if (((uint32_t)pData & 0x3) != 0) return FLASH_ERR_ALIGN;
  if ((size & 0x3) != 0) return FLASH_ERR_ALIGN;
  if (size > FLASH_RECORD_MAX) return FLASH_ERR_SIZE;
  return FLASH_OK;
}




Flash_Status_t Flash_Erase_Sector(void) {
  Flash_Status_t st = erase_data_sector_preserve_bl();
  LOG_I("FLASH", "Erase sector %d: %s", FLASH_DATA_SECTOR,
        st == FLASH_OK ? "OK" : "FAIL");
  return st;
}


Flash_Status_t Flash_Write(const uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  uint32_t record[FLASH_RECORD_MAX / 4 + 2];
  Flash_Record_Header_t hdr;
  hdr.magic    = FLASH_RECORD_MAGIC;
  hdr.datasize = dataSize;
  hdr.crc      = Flash_CRC32(pData, dataSize);

  
  memcpy(&record[0], &hdr, sizeof(hdr));
  memcpy((uint8_t *)&record[0] + sizeof(hdr), pData, dataSize);
  uint32_t total = sizeof(hdr) + dataSize;
  uint32_t words = (total + 3) / 4;

  st = erase_data_sector_preserve_bl();
  if (st != FLASH_OK) return st;

  st = program_words(FLASH_DATA_ADDR, record, words);
  LOG_I("FLASH", "Write %lu bytes: %s", (unsigned long)dataSize, st == FLASH_OK ? "OK" : "FAIL");
  return st;
}


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
  memcpy((uint8_t *)&buf[0] + sizeof(hdr), pData, dataSize);

  
  st = erase_sector(FLASH_BACKUP_SECTOR, FLASH_BACKUP_ADDR);
  if (st != FLASH_OK) return st;
  st = program_words(FLASH_BACKUP_ADDR, buf, words);
  if (st != FLASH_OK) return st;
  LOG_I("FLASH", "Backup saved (%lu bytes)", (unsigned long)total);

  
  st = erase_data_sector_preserve_bl();
  if (st != FLASH_OK) return st;
  st = program_words(FLASH_DATA_ADDR, buf, words);
  if (st != FLASH_OK) {
    LOG_E("FLASH", "Write FAILED — restoring from backup");
    
    for (uint32_t i = 0; i < words; i++)
      buf[i] = *(volatile uint32_t *)(FLASH_BACKUP_ADDR + i * 4);
    st = erase_data_sector_preserve_bl();
    if (st == FLASH_OK)
      program_words(FLASH_DATA_ADDR, buf, words);
    return FLASH_ERR_PROGRAM;
  }

  
  program_word(FLASH_BACKUP_ADDR, 0);
  LOG_I("FLASH", "Write with backup OK (%lu bytes)", (unsigned long)dataSize);
  return FLASH_OK;
}


bool Flash_Check_Backup(void) {
  volatile uint32_t *magic = (volatile uint32_t *)FLASH_BACKUP_ADDR;
  if (*magic != FLASH_RECORD_MAGIC) return false;

  LOG_W("FLASH", "Found backup, restoring...");
  uint32_t buf[FLASH_RECORD_MAX / 4 + 2];
  uint32_t words = FLASH_RECORD_MAX / 4 + 2;
  for (uint32_t i = 0; i < words; i++)
    buf[i] = *(volatile uint32_t *)(FLASH_BACKUP_ADDR + i * 4);

  Flash_Status_t st = erase_data_sector_preserve_bl();
  if (st == FLASH_OK) {
    program_words(FLASH_DATA_ADDR, buf, words);
    program_word(FLASH_BACKUP_ADDR, 0); 
    LOG_I("FLASH", "Backup restored OK");
  }
  return true;
}




static bool rolling_record_valid(uint32_t addr, Flash_Record_Header_t **out_hdr) {
  if (addr > FLASH_DATA_ADDR + FLASH_DATA_SIZE - FLASH_HDR_SIZE) {
    return false;
  }

  Flash_Record_Header_t *hdr = (Flash_Record_Header_t *)addr;
  if (hdr->magic != FLASH_RECORD_MAGIC) {
    return false;
  }
  if (hdr->datasize == 0U || hdr->datasize > FLASH_RECORD_MAX) {
    return false;
  }

  uint32_t total = FLASH_HDR_SIZE + hdr->datasize;
  if (addr + total > FLASH_DATA_ADDR + FLASH_DATA_SIZE) {
    return false;
  }

  const uint32_t *src = (const uint32_t *)(addr + FLASH_HDR_SIZE);
  if (Flash_CRC32(src, hdr->datasize) != hdr->crc) {
    return false;
  }

  if (out_hdr) {
    *out_hdr = hdr;
  }
  return true;
}

static bool flash_range_erased(uint32_t addr, uint32_t len) {
  const uint8_t *p = (const uint8_t *)addr;
  for (uint32_t i = 0; i < len; ++i) {
    if (p[i] != 0xFFU) {
      return false;
    }
  }
  return true;
}

static uint32_t rolling_find_last(void) {
  uint32_t last = FLASH_DATA_ADDR;
  uint32_t addr = FLASH_DATA_ADDR;
  bool found = false;

  while (addr < FLASH_DATA_ADDR + FLASH_DATA_SIZE - FLASH_HDR_SIZE) {
    Flash_Record_Header_t *hdr = NULL;
    if (!rolling_record_valid(addr, &hdr)) break;
    last = addr;
    found = true;
    uint32_t step = ALIGN4(FLASH_HDR_SIZE + hdr->datasize);
    if (step < FLASH_HDR_SIZE + 4U) break; /* safety */
    addr += step;
  }
  return found ? last : FLASH_DATA_ADDR;
}

Flash_Status_t Flash_Rolling_Write(const uint32_t *pData, uint32_t dataSize) {
  Flash_Status_t st = check_align(pData, dataSize);
  if (st != FLASH_OK) return st;

  uint32_t total = sizeof(Flash_Record_Header_t) + dataSize;
  uint32_t slot_size = ALIGN4(total);
  if (slot_size > FLASH_RECORD_MAX + sizeof(Flash_Record_Header_t)) return FLASH_ERR_SIZE;

  Flash_Record_Header_t hdr;
  hdr.magic    = FLASH_RECORD_MAGIC;
  hdr.crc      = Flash_CRC32(pData, dataSize);
  hdr.datasize = dataSize;

  uint32_t last = rolling_find_last();
  uint32_t next = FLASH_DATA_ADDR;
  Flash_Record_Header_t *last_hdr = NULL;
  if (rolling_record_valid(last, &last_hdr)) {
    uint32_t last_slot = ALIGN4(FLASH_HDR_SIZE + last_hdr->datasize);
    if (last_slot < FLASH_HDR_SIZE + 4U) {
      return FLASH_ERR_CRC;
    }
    next = last + last_slot;
  }

  
  if (next + slot_size > FLASH_DATA_ADDR + FLASH_DATA_SIZE) {
    LOG_W("FLASH", "Rolling sector full, erasing...");
    st = erase_data_sector_preserve_bl();
    if (st != FLASH_OK) return st;
    next = FLASH_DATA_ADDR;
  }

  if (!flash_range_erased(next, slot_size)) {
    LOG_W("FLASH", "Rolling slot dirty, erasing sector...");
    st = erase_data_sector_preserve_bl();
    if (st != FLASH_OK) return st;
    next = FLASH_DATA_ADDR;
  }

  st = program_words(next, (const uint32_t *)&hdr,
                     sizeof(hdr) / sizeof(uint32_t));
  if (st == FLASH_OK) {
    st = program_words(next + sizeof(hdr), pData, dataSize / sizeof(uint32_t));
  }
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

  
  Flash_Record_Header_t *valid_hdr = NULL;
  if (!rolling_record_valid(last, &valid_hdr)) {
    LOG_D("FLASH", "No valid rolling data found");
    if (outSize) *outSize = 0;
    return FLASH_ERR_CRC;
  }
  hdr = valid_hdr;

  uint32_t storedSize = hdr->datasize;
  if (storedSize > FLASH_RECORD_MAX) {
    LOG_E("FLASH", "Corrupt record: datasize=%lu exceeds max", (unsigned long)storedSize);
    return FLASH_ERR_CRC;
  }
  uint32_t copySize = maxSize < storedSize ? maxSize : storedSize;
  const uint8_t *src = (const uint8_t *)(last + sizeof(Flash_Record_Header_t));
  if (copySize < storedSize) {
    LOG_W("FLASH", "Rolling read truncated: stored=%lu max=%lu",
          (unsigned long)storedSize, (unsigned long)maxSize);
  }
  memcpy(pData, src, copySize);
  if (outSize) *outSize = copySize;
  LOG_I("FLASH", "Rolling read @0x%08lX %lu bytes OK", last, (unsigned long)copySize);
  return FLASH_OK;
}


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

/* ==================================================================
 * SD / FatFs formatting helper
 * ================================================================== */


#ifndef SFHD_SD_WORK_SECTOR_SIZE
#define SFHD_SD_WORK_SECTOR_SIZE 512U
#endif

/* Keep this buffer in normal SRAM, 4-byte aligned. 512 bytes is the minimum
 * work buffer accepted by this FatFs configuration (_MIN_SS == _MAX_SS == 512).
 * Do not put it in CCMRAM: SDIO DMA cannot access CCM on STM32F4.
 */
static uint32_t sfhd_sd_work[SFHD_SD_WORK_SECTOR_SIZE / sizeof(uint32_t)]
    __attribute__((aligned(4)));

/* FatFs stores the FATFS* pointer passed to f_mount(). It must stay valid after
 * SFHD_SD_FormatAndInit() returns, so do not use a stack FATFS object here.
 */
static FATFS sfhd_sd_fs;

static void sfhd_sd_progress(const SFHD_SD_FormatOptions_t *opt,
                             const char *step, FRESULT res) {
  LOG_I("SFHD", "SD format step: %s => %s(%d)", step ? step : "?",
        SFHD_FResultName(res), (int)res);
  if (opt != NULL && opt->progress != NULL) {
    opt->progress(step, res, opt->user);
  }
}

const char *SFHD_FResultName(FRESULT res) {
  switch (res) {
  case FR_OK: return "FR_OK";
  case FR_DISK_ERR: return "FR_DISK_ERR";
  case FR_INT_ERR: return "FR_INT_ERR";
  case FR_NOT_READY: return "FR_NOT_READY";
  case FR_NO_FILE: return "FR_NO_FILE";
  case FR_NO_PATH: return "FR_NO_PATH";
  case FR_INVALID_NAME: return "FR_INVALID_NAME";
  case FR_DENIED: return "FR_DENIED";
  case FR_EXIST: return "FR_EXIST";
  case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
  case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
  case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
  case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
  case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
  case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
  case FR_TIMEOUT: return "FR_TIMEOUT";
  case FR_LOCKED: return "FR_LOCKED";
  case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
  case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
  case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
  default: return "FR_UNKNOWN";
  }
}

uint32_t SFHD_FResultToSysError(FRESULT res) {
  switch (res) {
  case FR_OK: return SYS_ERR_NONE;
  case FR_NOT_READY: return SYS_ERR_SD_NOT_READY;
  case FR_TIMEOUT: return SYS_ERR_SD_TIMEOUT;
  case FR_DISK_ERR: return SYS_ERR_SD_DISK_ERR;
  case FR_INT_ERR: return SYS_ERR_SD_LOST;
  case FR_NO_FILESYSTEM: return SYS_ERR_SD_NO_FILESYSTEM;
  case FR_MKFS_ABORTED:
  case FR_INVALID_PARAMETER:
  case FR_DENIED:
  case FR_WRITE_PROTECTED:
  case FR_NOT_ENOUGH_CORE:
  default:
    return SYS_ERR_SD_FORMAT_FAILED;
  }
}

void SFHD_SD_DebugProbe(const char *tag) {
  DSTATUS st = disk_status(0);
  DWORD sector_count = 0;
  WORD sector_size = 0;
  DWORD block_size = 0;
  DRESULT rc_count = RES_ERROR;
  DRESULT rc_size = RES_ERROR;
  DRESULT rc_block = RES_ERROR;

  LOG_I("SFHD", "[%s] disk_status=0x%02X", tag ? tag : "probe", (unsigned)st);

  st = disk_initialize(0);
  LOG_I("SFHD", "[%s] disk_initialize=0x%02X", tag ? tag : "probe", (unsigned)st);

  if ((st & STA_NOINIT) == 0U) {
    rc_count = disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);
    rc_size = disk_ioctl(0, GET_SECTOR_SIZE, &sector_size);
    rc_block = disk_ioctl(0, GET_BLOCK_SIZE, &block_size);
    LOG_I("SFHD", "[%s] ioctl count rc=%d value=%lu", tag ? tag : "probe",
          (int)rc_count, (unsigned long)sector_count);
    LOG_I("SFHD", "[%s] ioctl ssize rc=%d value=%u", tag ? tag : "probe",
          (int)rc_size, (unsigned)sector_size);
    LOG_I("SFHD", "[%s] ioctl block rc=%d value=%lu", tag ? tag : "probe",
          (int)rc_block, (unsigned long)block_size);
  }
}

static FRESULT sfhd_mount(FATFS *fs) {
  FRESULT res = f_mount(fs, "0:", 1);
  LOG_I("SFHD", "f_mount(0:) => %s(%d)", SFHD_FResultName(res), (int)res);
  return res;
}

static FRESULT sfhd_unmount(void) {
  FRESULT res = f_mount(NULL, "0:", 0);
  LOG_I("SFHD", "f_mount(NULL) => %s(%d)", SFHD_FResultName(res), (int)res);
  return res;
}

static FRESULT sfhd_mkdir_ok_exist(const char *path) {
  FRESULT res = f_mkdir(path);
  LOG_I("SFHD", "f_mkdir(%s) => %s(%d)", path, SFHD_FResultName(res), (int)res);
  if (res == FR_EXIST) {
    return FR_OK;
  }
  return res;
}

static FRESULT sfhd_write_init_marker(void) {
  FIL file;
  UINT bw = 0;
  const char init_text[] = "TOS_SD_INIT=1\nVERSION=1\n";

  FRESULT res = f_open(&file, "0:/init", FA_CREATE_ALWAYS | FA_WRITE);
  LOG_I("SFHD", "f_open(0:/init) => %s(%d)", SFHD_FResultName(res), (int)res);
  if (res != FR_OK) {
    return res;
  }

  res = f_write(&file, init_text, sizeof(init_text) - 1U, &bw);
  LOG_I("SFHD", "f_write(init) => %s(%d), bw=%u", SFHD_FResultName(res),
        (int)res, (unsigned)bw);
  if (res == FR_OK) {
    res = f_sync(&file);
    LOG_I("SFHD", "f_sync(init) => %s(%d)", SFHD_FResultName(res), (int)res);
  }

  FRESULT close_res = f_close(&file);
  LOG_I("SFHD", "f_close(init) => %s(%d)", SFHD_FResultName(close_res),
        (int)close_res);

  if (res != FR_OK) {
    return res;
  }
  if (close_res != FR_OK) {
    return close_res;
  }
  return (bw == sizeof(init_text) - 1U) ? FR_OK : FR_DISK_ERR;
}

static FRESULT sfhd_create_layout(void) {
  static const char *dirs[] = {
      "0:/data",
      "0:/oem",
      "0:/dev",
      "0:/storage",
      "0:/system",
  };

  for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
    FRESULT res = sfhd_mkdir_ok_exist(dirs[i]);
    if (res != FR_OK) {
      return res;
    }
  }

  return sfhd_write_init_marker();
}

static FRESULT sfhd_dresult_to_fresult(DRESULT rc) {
  switch (rc) {
  case RES_OK: return FR_OK;
  case RES_NOTRDY: return FR_NOT_READY;
  case RES_WRPRT: return FR_WRITE_PROTECTED;
  case RES_PARERR: return FR_INVALID_PARAMETER;
  case RES_ERROR:
  default: return FR_DISK_ERR;
  }
}

static FRESULT sfhd_write_preflight(void) {
  /* The user has already confirmed formatting, so sector 0 is allowed to be
   * overwritten.  This catches a broken SD write path before f_mkfs() spends
   * tens of seconds inside the formatter.
   */
  BYTE *buf = (BYTE *)sfhd_sd_work;
  memset(buf, 0xA5, sizeof(sfhd_sd_work));
  memcpy(buf, "TOSFMT", 6);

  uint32_t t0 = HAL_GetTick();
  DRESULT wr = disk_write(0, buf, 0, 1);
  LOG_I("SFHD", "preflight disk_write sector0 => rc=%d dt=%lums",
        (int)wr, (unsigned long)(HAL_GetTick() - t0));
  if (wr != RES_OK) {
    return sfhd_dresult_to_fresult(wr);
  }

  DRESULT sync = disk_ioctl(0, CTRL_SYNC, NULL);
  LOG_I("SFHD", "preflight CTRL_SYNC => rc=%d", (int)sync);
  if (sync != RES_OK) {
    return sfhd_dresult_to_fresult(sync);
  }

  memset(buf, 0, sizeof(sfhd_sd_work));
  t0 = HAL_GetTick();
  DRESULT rd = disk_read(0, buf, 0, 1);
  LOG_I("SFHD", "preflight disk_read sector0 => rc=%d dt=%lums sig=%02X %02X %02X %02X %02X %02X",
        (int)rd, (unsigned long)(HAL_GetTick() - t0),
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  if (rd != RES_OK) {
    return sfhd_dresult_to_fresult(rd);
  }
  if (memcmp(buf, "TOSFMT", 6) != 0) {
    LOG_E("SFHD", "preflight readback mismatch");
    return FR_DISK_ERR;
  }

  return FR_OK;
}

static void sfhd_recover_disk_after_error(const char *tag) {
  LOG_W("SFHD", "recover disk after error: %s", tag ? tag : "?");
  (void)disk_ioctl(0, CTRL_SYNC, NULL);
  (void)disk_initialize(0);
  SFHD_SD_DebugProbe(tag ? tag : "recover");
}

static FRESULT sfhd_try_mkfs(BYTE opt, const char *name) {
  memset(sfhd_sd_work, 0, sizeof(sfhd_sd_work));
  LOG_I("SFHD", "f_mkfs start: %s opt=0x%02X work=%lu", name, (unsigned)opt,
        (unsigned long)sizeof(sfhd_sd_work));
  uint32_t t0 = HAL_GetTick();
  FRESULT res = f_mkfs("0:", opt, 0, sfhd_sd_work, sizeof(sfhd_sd_work));
  LOG_I("SFHD", "f_mkfs done : %s => %s(%d), dt=%lums", name,
        SFHD_FResultName(res), (int)res,
        (unsigned long)(HAL_GetTick() - t0));
  return res;
}

FRESULT SFHD_SD_FormatAndInit(const SFHD_SD_FormatOptions_t *options) {
  /* If SD card is hard-disabled, bail out immediately — avoid touching
   * non-existent hardware and triggering SysHandle_Exception. */
  extern bool TSDIO_IsHardDisabled(void);
  if (TSDIO_IsHardDisabled()) {
    LOG_W("SFHD", "SD format blocked: SD card is hard-disabled");
    return FR_NOT_READY;
  }

  DWORD free_clusters = 0;
  FATFS *mounted_fs = NULL;

  sfhd_sd_progress(options, "unmount", FR_OK);
  (void)sfhd_unmount();

  sfhd_sd_progress(options, "probe", FR_OK);
  SFHD_SD_DebugProbe("before-format");

  DSTATUS st = disk_initialize(0);
  if (st & STA_NOINIT) {
    LOG_E("SFHD", "disk_initialize failed: status=0x%02X", (unsigned)st);
    return FR_NOT_READY;
  }
  if (st & STA_PROTECT) {
    LOG_E("SFHD", "disk is write-protected: status=0x%02X", (unsigned)st);
    return FR_WRITE_PROTECTED;
  }

  sfhd_sd_progress(options, "write preflight", FR_OK);
  FRESULT res = sfhd_write_preflight();
  if (res != FR_OK) {
    sfhd_sd_progress(options, "write preflight failed", res);
    return res;
  }

  /* FatFs R0.14 f_mkfs() requires opt to include FM_FAT/FM_FAT32. Passing 0
   * returns FR_INVALID_PARAMETER in this package. First try a partitioned FAT
   * volume, then fall back to SFD. SFD is useful for cards/readers that dislike
   * the 63-sector offset used by the partitioned mode.
   */
  sfhd_sd_progress(options, "mkfs partition", FR_OK);
  res = sfhd_try_mkfs((BYTE)(FM_FAT | FM_FAT32), "partitioned");

  if (res != FR_OK) {
    sfhd_sd_progress(options, "mkfs sfd retry", res);
    sfhd_recover_disk_after_error("before-sfd-retry");
    res = sfhd_try_mkfs((BYTE)(FM_FAT | FM_FAT32 | FM_SFD), "sfd");
  }

  if (res != FR_OK) {
    sfhd_sd_progress(options, "mkfs failed", res);
    return res;
  }

  (void)disk_ioctl(0, CTRL_SYNC, NULL);

  sfhd_sd_progress(options, "mount", FR_OK);
  res = sfhd_mount(&sfhd_sd_fs);
  if (res != FR_OK) {
    sfhd_sd_progress(options, "mount failed", res);
    return res;
  }

  /* Force a metadata read. This catches broken cards earlier than entering the
   * file browser and gives us a useful log line.
   */
  res = f_getfree("0:", &free_clusters, &mounted_fs);
  LOG_I("SFHD", "f_getfree => %s(%d), free_clusters=%lu, csize=%lu",
        SFHD_FResultName(res), (int)res, (unsigned long)free_clusters,
        mounted_fs ? (unsigned long)mounted_fs->csize : 0UL);
  if (res != FR_OK) {
    sfhd_sd_progress(options, "verify failed", res);
    return res;
  }

  sfhd_sd_progress(options, "create layout", FR_OK);
  res = sfhd_create_layout();
  if (res != FR_OK) {
    sfhd_sd_progress(options, "layout failed", res);
    return res;
  }

  (void)disk_ioctl(0, CTRL_SYNC, NULL);
  sfhd_sd_progress(options, "complete", FR_OK);
  SFHD_SD_DebugProbe("after-format");
  return FR_OK;
}
