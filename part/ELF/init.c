#include "init.h"

#include <stddef.h>
#include <stdint.h>

#include "stm32f407xx.h"
#include "tee_format.h"
#include "tos_partitions.h"

#define ELF_FLASH_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                          FLASH_SR_PGPERR | FLASH_SR_PGSERR)

static uint8_t elf_flash_wait(uint32_t guard) {
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    if (--guard == 0U) return 0U;
  }
  return 1U;
}

static uint8_t elf_flash_unlock(void) {
  if (!elf_flash_wait(0x0FFFFFFFUL)) return 0U;
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
  return (FLASH->CR & FLASH_CR_LOCK) == 0U ? 1U : 0U;
}

static void elf_flash_lock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

static void elf_flash_clear(void) {
  FLASH->SR = FLASH_SR_EOP | ELF_FLASH_ERRORS;
}

static void elf_flash_flush(void) {
  uint32_t acr = FLASH->ACR;
  FLASH->ACR = acr & ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
  FLASH->ACR |= FLASH_ACR_ICRST | FLASH_ACR_DCRST;
  FLASH->ACR &= ~(FLASH_ACR_ICRST | FLASH_ACR_DCRST);
  FLASH->ACR = acr;
  __DSB();
  __ISB();
}

static uint8_t elf_flash_erase_sector(uint32_t sector) {
  if (!elf_flash_wait(0x0FFFFFFFUL)) return 0U;
  elf_flash_clear();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_SER | FLASH_CR_MER);
  FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_SER |
               ((sector << FLASH_CR_SNB_Pos) & FLASH_CR_SNB);
  FLASH->CR |= FLASH_CR_STRT;
  if (!elf_flash_wait(0xFFFFFFFFUL)) {
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0U;
  }
  FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
  if ((FLASH->SR & ELF_FLASH_ERRORS) != 0U) {
    elf_flash_clear();
    return 0U;
  }
  return 1U;
}

static uint8_t elf_flash_program_word(uint32_t address, uint32_t value) {
  if ((address & 3U) != 0U || !elf_flash_wait(0x0FFFFFFFUL)) return 0U;
  elf_flash_clear();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_PG);
  FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_PG;
  *(volatile uint32_t *)address = value;
  if (!elf_flash_wait(0x0FFFFFFFUL)) {
    FLASH->CR &= ~FLASH_CR_PG;
    return 0U;
  }
  FLASH->CR &= ~FLASH_CR_PG;
  if ((FLASH->SR & ELF_FLASH_ERRORS) != 0U) {
    elf_flash_clear();
    return 0U;
  }
  return *(volatile const uint32_t *)address == value ? 1U : 0U;
}

static uint32_t elf_crc32(const void *data, uint32_t size) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  while (size-- != 0U) {
    crc ^= *p++;
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

static uint8_t elf_record_erased(const TosTeeStateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0U; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (w[i] != 0xFFFFFFFFUL) return 0U;
  }
  return 1U;
}

static const TosTeeStateRecord *elf_latest_state(void) {
  const TosTeeStateRecord *latest = NULL;
  for (uint32_t off = 0U; off + sizeof(TosTeeStateRecord) <= TOS_TEE_STATE_SIZE;
       off += sizeof(TosTeeStateRecord)) {
    const TosTeeStateRecord *r =
        (const TosTeeStateRecord *)(TOS_TEE_STATE_ADDRESS + off);
    if (elf_record_erased(r)) break;
    if (TosTeeStateRecordValid(r) && (!latest || r->sequence >= latest->sequence)) {
      latest = r;
    }
  }
  return latest;
}

static uint8_t elf_manifest_present(void) {
  const TosTeeManifest *m = (const TosTeeManifest *)TOS_PART_TEE_ADDRESS;
  return m->magic == TOS_TEE_MANIFEST_MAGIC &&
         m->version == TOS_TEE_MANIFEST_VERSION ? 1U : 0U;
}

static uint8_t elf_vector_valid(uint32_t address, uint32_t size) {
  const uint32_t *v = (const uint32_t *)address;
  uint32_t sp = v[0];
  uint32_t pc = v[1];
  if (sp < 0x20000000UL || sp > 0x20020000UL || (sp & 7U) != 0U) return 0U;
  if ((pc & 1U) == 0U) return 0U;
  pc &= ~1UL;
  return pc >= address && pc < address + size ? 1U : 0U;
}

static uint8_t elf_transaction_bounds(const TosTeeStateRecord *r) {
  if (r->source_address != TOS_TMP_STAGE_ADDRESS || r->image_size == 0U ||
      r->image_size > TOS_TMP_STAGE_SIZE) return 0U;
  if (r->update_kind == TOS_UPDATE_SBL) {
    return r->target_address == TOS_PART_SBL_ADDRESS &&
           r->image_size == TOS_PART_SBL_SIZE ? 1U : 0U;
  }
  if (r->update_kind == TOS_UPDATE_REC) {
    return r->target_address == TOS_PART_REC_ADDRESS &&
           r->image_size == TOS_PART_REC_SIZE ? 1U : 0U;
  }
  return 0U;
}

static uint8_t elf_mark_state(const TosTeeStateRecord *r, uint32_t state) {
  uint32_t address = (uint32_t)(uintptr_t)&r->txn_state;
  uint8_t ok = 0U;
  if (!elf_flash_unlock()) return 0U;
  ok = elf_flash_program_word(address, state);
  elf_flash_lock();
  elf_flash_flush();
  return ok;
}

static uint8_t elf_apply_update(const TosTeeStateRecord *r) {
  const uint32_t *src = (const uint32_t *)r->source_address;
  uint32_t words = r->image_size / 4U;

  if (!elf_transaction_bounds(r) || !elf_manifest_present() ||
      elf_crc32(src, r->image_size) != r->image_crc32) {
    (void)elf_mark_state(r, TOS_TXN_STATE_FAILED);
    return 0U;
  }

  if (!elf_flash_unlock()) return 0U;
  if (r->update_kind == TOS_UPDATE_SBL) {
    if (!elf_flash_erase_sector(1U) || !elf_flash_erase_sector(2U)) {
      elf_flash_lock();
      return 0U;
    }
  } else if (!elf_flash_erase_sector(4U)) {
    elf_flash_lock();
    return 0U;
  }

  for (uint32_t i = 0U; i < words; ++i) {
    if (!elf_flash_program_word(r->target_address + i * 4U, src[i])) {
      elf_flash_lock();
      return 0U;
    }
  }
  elf_flash_lock();
  elf_flash_flush();

  if (elf_crc32((const void *)(uintptr_t)r->target_address, r->image_size) !=
      r->image_crc32) {
    return 0U;
  }
  return elf_mark_state(r, TOS_TXN_STATE_DONE);
}

static void elf_reset(void) {
  __disable_irq();
  __DSB();
  SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
  __DSB();
  while (1) __NOP();
}

static void elf_jump(uint32_t address) {
  const uint32_t *v = (const uint32_t *)address;
  void (*entry)(void) = (void (*)(void))(uintptr_t)v[1];
  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  for (uint32_t i = 0U; i < 8U; ++i) {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }
  SCB->VTOR = address;
  __set_MSP(v[0]);
  __set_CONTROL(0U);
  __DSB();
  __ISB();
  entry();
  while (1) __NOP();
}

void ELF_Main(void) {
  const TosTeeStateRecord *state;
  SCB->VTOR = TOS_PART_ELF_ADDRESS;
  state = elf_latest_state();
  if (state && state->txn_state == TOS_TXN_STATE_PENDING &&
      state->update_kind != TOS_UPDATE_NONE) {
    if (elf_apply_update(state)) elf_reset();
    /* Bounds/CRC failures are marked FAILED before the target is touched.
     * Keep the device recoverable by returning to the existing SBL.  Erase or
     * programming failures remain PENDING and intentionally stop here so the
     * intact TMP image can be retried after a clean power cycle. */
    if (state->txn_state == TOS_TXN_STATE_FAILED &&
        elf_vector_valid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_SIZE)) {
      elf_jump(TOS_PART_SBL_ADDRESS);
    }
    /* A transient erase/program failure is deliberately left pending so a
     * clean power cycle can retry from the intact TMP image. */
    while (1) __NOP();
  }
  if (elf_vector_valid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_SIZE)) {
    elf_jump(TOS_PART_SBL_ADDRESS);
  }
  while (1) __NOP();
}
