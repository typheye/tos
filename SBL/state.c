#include "main.h"
#include "sbl_state.h"

#define SBL_STATE_MAGIC   0x53424C55UL /* SBLU */
#define SBL_STATE_VERSION 1UL

extern const uint32_t __sbl_state_start__[];
#define SBL_STATE_ADDR ((uint32_t)__sbl_state_start__)

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t unlocked;
  uint32_t crc;
} SBL_StateRecord;

static SBL_CODE uint32_t sbl_state_crc(const SBL_StateRecord *r) {
  return r->magic ^ r->version ^ r->unlocked ^ 0xA5A55A5AUL;
}

SBL_CODE uint8_t SBL_StateUnlocked(void) {
  const SBL_StateRecord *r = (const SBL_StateRecord *)SBL_STATE_ADDR;
  if (r->magic != SBL_STATE_MAGIC || r->version != SBL_STATE_VERSION) {
    return 0U;
  }
  if (r->crc != sbl_state_crc(r)) {
    return 0U;
  }
  return r->unlocked ? 1U : 0U;
}

SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked) {
  SBL_StateRecord r;
  r.magic = SBL_STATE_MAGIC;
  r.version = SBL_STATE_VERSION;
  r.unlocked = unlocked ? 1UL : 0UL;
  r.crc = sbl_state_crc(&r);

  HAL_FLASH_Unlock();
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR |
                         FLASH_FLAG_PGSERR);

  FLASH_EraseInitTypeDef erase;
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = FLASH_SECTOR_1;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  uint32_t err = 0U;
  if (HAL_FLASHEx_Erase(&erase, &err) != HAL_OK) {
    HAL_FLASH_Lock();
    return 0U;
  }

  const uint32_t *words = (const uint32_t *)&r;
  for (uint32_t i = 0U; i < sizeof(r) / sizeof(uint32_t); ++i) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                          SBL_STATE_ADDR + i * sizeof(uint32_t),
                          words[i]) != HAL_OK) {
      HAL_FLASH_Lock();
      return 0U;
    }
  }

  HAL_FLASH_Lock();
  return 1U;
}
