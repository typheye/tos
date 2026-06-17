#include "main.h"
#include "sbl_state.h"

#define SBL_STATE_MAGIC   0x53424C55UL /* SBLU */
#define SBL_STATE_VERSION 2UL
#define SBL_STATE_VERSION_OLD 1UL
#define SBL_STATE_OLD_ADDR 0x0800FC00UL
#define SBL_STATE_AREA_SIZE 1024UL
#define SBL_STATE_SECTOR_INDEX 3UL

extern const uint32_t __sbl_state_start__[];
#define SBL_STATE_ADDR ((uint32_t)__sbl_state_start__)

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t unlocked;
  uint32_t boot_target;
  uint32_t crc;
} SBL_StateRecord;

static SBL_CODE uint32_t sbl_state_crc(const SBL_StateRecord *r) {
  return r->magic ^ r->version ^ r->unlocked ^ r->boot_target ^
         0xA5A55A5AUL;
}

static SBL_CODE uint32_t sbl_state_crc_old(const SBL_StateRecord *r) {
  return r->magic ^ r->version ^ r->unlocked ^ 0xA5A55A5AUL;
}

static SBL_CODE uint8_t sbl_state_valid(const SBL_StateRecord *r) {
  if (r->magic != SBL_STATE_MAGIC) {
    return 0U;
  }
  if (r->version == SBL_STATE_VERSION) {
    return (r->crc == sbl_state_crc(r)) ? 1U : 0U;
  }
  if (r->version == SBL_STATE_VERSION_OLD) {
    return (r->boot_target == sbl_state_crc_old(r)) ? 1U : 0U;
  }
  return 0U;
}

static SBL_CODE uint8_t sbl_state_erased(const SBL_StateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0U; i < sizeof(SBL_StateRecord) / sizeof(uint32_t); i++) {
    if (w[i] != 0xFFFFFFFFUL) {
      return 0U;
    }
  }
  return 1U;
}

static SBL_CODE const SBL_StateRecord *sbl_state_latest(void) {
  const SBL_StateRecord *latest = 0;
  for (uint32_t off = 0U; off + sizeof(SBL_StateRecord) <= SBL_STATE_AREA_SIZE;
       off += sizeof(SBL_StateRecord)) {
    const SBL_StateRecord *r = (const SBL_StateRecord *)(SBL_STATE_ADDR + off);
    if (sbl_state_erased(r)) {
      break;
    }
    if (sbl_state_valid(r)) {
      latest = r;
    }
  }
  if (latest) {
    return latest;
  }

  const SBL_StateRecord *old = (const SBL_StateRecord *)SBL_STATE_OLD_ADDR;
  return sbl_state_valid(old) ? old : 0;
}

SBL_CODE uint8_t SBL_StateUnlocked(void) {
  const SBL_StateRecord *r = sbl_state_latest();
  return (r && r->unlocked) ? 1U : 0U;
}

static SBL_CODE uint8_t sbl_state_program(uint32_t addr,
                                          const SBL_StateRecord *r) {
  const uint32_t *words = (const uint32_t *)r;
  if (!SBL_FlashUnlock()) {
    return 0U;
  }
  for (uint32_t i = 0U; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (!SBL_FlashProgramWord(addr + i * sizeof(uint32_t), words[i])) {
      SBL_FlashLock();
      return 0U;
    }
  }
  SBL_FlashLock();
  return 1U;
}

static SBL_CODE uint8_t sbl_state_erase_area(void) {
  uint8_t ok;
  if (!SBL_FlashUnlock()) {
    return 0U;
  }
  ok = SBL_FlashEraseSectorIndex(SBL_STATE_SECTOR_INDEX);
  SBL_FlashLock();
  return ok;
}

static SBL_CODE uint8_t sbl_state_write(uint8_t unlocked, uint32_t boot_target) {
  SBL_StateRecord r;
  r.magic = SBL_STATE_MAGIC;
  r.version = SBL_STATE_VERSION;
  r.unlocked = unlocked ? 1UL : 0UL;
  r.boot_target = boot_target;
  r.crc = sbl_state_crc(&r);

  uint32_t addr = 0U;
  for (uint32_t off = 0U; off + sizeof(SBL_StateRecord) <= SBL_STATE_AREA_SIZE;
       off += sizeof(SBL_StateRecord)) {
    const SBL_StateRecord *slot =
        (const SBL_StateRecord *)(SBL_STATE_ADDR + off);
    if (sbl_state_erased(slot)) {
      addr = SBL_STATE_ADDR + off;
      break;
    }
  }
  if (addr == 0U) {
    if (!sbl_state_erase_area()) {
      return 0U;
    }
    addr = SBL_STATE_ADDR;
  }

  if (sbl_state_program(addr, &r)) {
    return 1U;
  }

  if (!sbl_state_erase_area()) {
    return 0U;
  }
  return sbl_state_program(SBL_STATE_ADDR, &r);
}

SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked) {
  return sbl_state_write(unlocked, SBL_BOOT_TARGET_RECOVERY_FORMAT);
}

SBL_CODE uint8_t SBL_StateSetBootTarget(uint32_t target) {
  return sbl_state_write(SBL_StateUnlocked(), target);
}

SBL_CODE uint32_t SBL_StateConsumeBootTarget(void) {
  const SBL_StateRecord *r = sbl_state_latest();
  uint32_t target;
  if (!r || r->version != SBL_STATE_VERSION) {
    return SBL_BOOT_TARGET_NONE;
  }
  target = r->boot_target;
  if (target == SBL_BOOT_TARGET_FASTBOOT ||
      target == SBL_BOOT_TARGET_RECOVERY ||
      target == SBL_BOOT_TARGET_RECOVERY_FORMAT ||
      target == SBL_BOOT_TARGET_RECOVERY_UPGRADE) {
    (void)sbl_state_write(r->unlocked ? 1U : 0U, SBL_BOOT_TARGET_NONE);
    return target;
  }
  return SBL_BOOT_TARGET_NONE;
}
