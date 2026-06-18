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
#include "hardware/include/flash_diskio.h"
#include "tee_format.h"



#define FLASH_TIMEOUT    500u   
#define ALIGN4(x)        (((uint32_t)(x) + 3u) & ~3u)
#define FLASH_BL_REC_ADDR TOS_PART_REC_ADDRESS


static __attribute__((unused)) Flash_Status_t wait_ready(uint32_t timeout) {
  uint32_t tick = HAL_GetTick();
  while (HAL_FLASH_GetError() != 0) {
    if (HAL_GetTick() - tick > timeout) return FLASH_ERR_TIMEOUT;
  }
  return FLASH_OK;
}


static inline void flash_unlock(void) { HAL_FLASH_Unlock(); }
static inline void flash_lock(void)   { HAL_FLASH_Lock(); }


static Flash_Status_t erase_sector(uint32_t sector) {
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

static bool flash_bl_state_erased(const TosTeeStateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (w[i] != 0xFFFFFFFFu) {
      return false;
    }
  }
  return true;
}

static const TosTeeStateRecord *flash_bl_state_latest(void) {
  const TosTeeStateRecord *latest = NULL;
  for (uint32_t off = 0U; off + sizeof(TosTeeStateRecord) <= FLASH_BL_STATE_SIZE;
       off += sizeof(TosTeeStateRecord)) {
    const TosTeeStateRecord *r =
        (const TosTeeStateRecord *)(FLASH_BL_STATE_ADDR + off);
    if (flash_bl_state_erased(r)) break;
    if (TosTeeStateRecordValid(r) &&
        (!latest || r->sequence >= latest->sequence)) latest = r;
  }
  return latest;
}

static Flash_Status_t flash_bl_state_append(uint32_t boot_target) {
  TosTeeStateRecord r;
  const TosTeeStateRecord *latest = flash_bl_state_latest();
  uint32_t addr = 0U;
  for (uint32_t off = 0U; off + sizeof(r) <= FLASH_BL_STATE_SIZE;
       off += sizeof(r)) {
    const TosTeeStateRecord *slot =
        (const TosTeeStateRecord *)(FLASH_BL_STATE_ADDR + off);
    if (flash_bl_state_erased(slot)) {
      addr = FLASH_BL_STATE_ADDR + off;
      break;
    }
  }
  /* Sector 3 also contains the immutable TEE manifest/signatures. Never erase
   * it from SYSTEM merely to reclaim the append-only transaction tail. */
  if (addr == 0U) return FLASH_ERR_SIZE;

  r.magic = TOS_TEE_STATE_MAGIC;
  r.version = TOS_TEE_STATE_VERSION;
  r.sequence = latest ? latest->sequence + 1U : 1U;
  r.unlocked = latest && latest->unlocked ? 1U : 0U;
  r.boot_target = boot_target;
  r.update_kind = TOS_UPDATE_NONE;
  r.txn_state = 0xFFFFFFFFUL;
  r.source_address = 0xFFFFFFFFUL;
  r.target_address = 0xFFFFFFFFUL;
  r.image_size = 0U;
  r.image_crc32 = 0xFFFFFFFFUL;
  r.post_boot_target = TOS_BOOT_TARGET_NONE;
  r.reserved0 = 0xFFFFFFFFUL;
  r.reserved1 = 0xFFFFFFFFUL;
  r.reserved2 = 0xFFFFFFFFUL;
  r.record_crc = TosTeeStateRecordCrc(&r);
  return program_words(addr, (const uint32_t *)&r,
                       sizeof(r) / sizeof(uint32_t));
}

Flash_Status_t Flash_BL_SetBootTarget(uint32_t target) {
  if (target != FLASH_BL_BOOT_FASTBOOT &&
      target != FLASH_BL_BOOT_RECOVERY &&
      target != FLASH_BL_BOOT_RECOVERY_FORMAT &&
      target != FLASH_BL_BOOT_RECOVERY_UPGRADE &&
      target != FLASH_BL_BOOT_RECOVERY_INIT &&
      target != FLASH_BL_BOOT_NONE) {
    return FLASH_ERR_SIZE;
  }
  return flash_bl_state_append(target);
}

bool Flash_BL_RecoveryAvailable(void) {
  uint32_t msp = *(const uint32_t *)FLASH_BL_REC_ADDR;
  uint32_t reset = *(const uint32_t *)(FLASH_BL_REC_ADDR + 4U);
  uint32_t reset_addr = reset & ~1UL;
  return msp >= 0x20000000UL && msp <= 0x20020000UL &&
         (msp & 7U) == 0U && (reset & 1U) != 0U &&
         reset_addr >= FLASH_BL_REC_ADDR &&
         reset_addr < FLASH_BL_REC_ADDR + TOS_PART_REC_SIZE;
}

static Flash_Status_t erase_data_sector_preserve_bl(void) {
  Flash_Status_t st = erase_sector(FLASH_DATA_SECTOR);
  if (st != FLASH_OK) return st;
  return FlashDiskIO_RebuildUserdataAfterErase() ? FLASH_OK : FLASH_ERR_PROGRAM;
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
  /* Sector 10 is now the TMP transaction volume and must never be used as an
   * implicit settings backup. The append-only USERDATA journal already
   * provides power-loss tolerant settings updates. */
  return Flash_Rolling_Write(pData, dataSize);
}


bool Flash_Check_Backup(void) {
  return false;
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
