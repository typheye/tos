#include "main.h"
#include "sbl_state.h"

#define SBL_STATE_MAGIC   0x53424C55UL /* SBLU */
#define SBL_STATE_VERSION 1UL
#define SBL_STATE_OLD_ADDR 0x08007C00UL
#define SBL_STATE_AREA_SIZE 1024UL

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

static SBL_CODE uint8_t sbl_state_valid(const SBL_StateRecord *r) {
  if (r->magic != SBL_STATE_MAGIC || r->version != SBL_STATE_VERSION) {
    return 0U;
  }
  if (r->crc != sbl_state_crc(r)) {
    return 0U;
  }
  return 1U;
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

SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked) {
  SBL_StateRecord r;
  r.magic = SBL_STATE_MAGIC;
  r.version = SBL_STATE_VERSION;
  r.unlocked = unlocked ? 1UL : 0UL;
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
    return 0U;
  }

  if (!SBL_FlashUnlock()) {
    return 0U;
  }

  const uint32_t *words = (const uint32_t *)&r;
  for (uint32_t i = 0U; i < sizeof(r) / sizeof(uint32_t); ++i) {
    if (!SBL_FlashProgramWord(addr + i * sizeof(uint32_t), words[i])) {
      SBL_FlashLock();
      return 0U;
    }
  }

  SBL_FlashLock();
  return 1U;
}
