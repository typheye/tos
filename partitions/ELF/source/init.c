/**
 ******************************************************************************
 * @file    init.c
 * @author  Typheye
 * @brief   ELF root-of-trust boot and state management implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "init.h"

#include "stm32f407xx.h"
#include "secure_boot.h"
#include "manifest.h"
#include "cust.h"

#define ELF_FLASH_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                          FLASH_SR_PGPERR | FLASH_SR_PGSERR)
#define ELF_TXN_STATE_CONSUMED 0xFFFFFFE0UL

static uint8_t ELF_FlashWait(uint32_t guard) {
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    if (--guard == 0U) return 0U;
  }
  return 1U;
}

static uint8_t ELF_FlashUnlock(void) {
  if (!ELF_FlashWait(0x0FFFFFFFUL)) return 0U;
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
  return (FLASH->CR & FLASH_CR_LOCK) == 0U ? 1U : 0U;
}

static void ELF_FlashLock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

static void ELF_FlashClear(void) {
  FLASH->SR = FLASH_SR_EOP | ELF_FLASH_ERRORS;
}

static void ELF_FlashFlush(void) {
  uint32_t acr = FLASH->ACR;
  FLASH->ACR = acr & ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
  FLASH->ACR |= FLASH_ACR_ICRST | FLASH_ACR_DCRST;
  FLASH->ACR &= ~(FLASH_ACR_ICRST | FLASH_ACR_DCRST);
  FLASH->ACR = acr;
  __DSB();
  __ISB();
}

static uint8_t ELF_FlashEraseSector(uint32_t sector) {
  if (!ELF_FlashWait(0x0FFFFFFFUL)) return 0U;
  ELF_FlashClear();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_SER | FLASH_CR_MER);
  FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_SER |
               ((sector << FLASH_CR_SNB_Pos) & FLASH_CR_SNB);
  FLASH->CR |= FLASH_CR_STRT;
  if (!ELF_FlashWait(0xFFFFFFFFUL)) {
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0U;
  }
  FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
  if ((FLASH->SR & ELF_FLASH_ERRORS) != 0U) {
    ELF_FlashClear();
    return 0U;
  }
  return 1U;
}

static uint8_t ELF_FlashProgramWord(uint32_t address, uint32_t value) {
  if ((address & 3U) != 0U || !ELF_FlashWait(0x0FFFFFFFUL)) return 0U;
  ELF_FlashClear();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_PG);
  FLASH->CR |= FLASH_CR_PSIZE_1 | FLASH_CR_PG;
  *(volatile uint32_t *)address = value;
  if (!ELF_FlashWait(0x0FFFFFFFUL)) {
    FLASH->CR &= ~FLASH_CR_PG;
    return 0U;
  }
  FLASH->CR &= ~FLASH_CR_PG;
  if ((FLASH->SR & ELF_FLASH_ERRORS) != 0U) {
    ELF_FlashClear();
    return 0U;
  }
  return *(volatile const uint32_t *)address == value ? 1U : 0U;
}

static uint32_t ELF_Crc32(const void *data, uint32_t size) {
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

static uint8_t ELF_RecordErased(const TosTeeStateRecord_t *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0U; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (w[i] != 0xFFFFFFFFUL) return 0U;
  }
  return 1U;
}

static const TosTeeStateRecord_t *ELF_LatestState(void) {
  const TosTeeStateRecord_t *latest = NULL;
  for (uint32_t off = 0U; off + sizeof(TosTeeStateRecord_t) <= TOS_TEE_STATE_SIZE;
       off += sizeof(TosTeeStateRecord_t)) {
    const TosTeeStateRecord_t *r =
        (const TosTeeStateRecord_t *)(TOS_TEE_STATE_ADDRESS + off);
    if (ELF_RecordErased(r)) break;
    if (!TosTeeStateRecordValid(r)) continue;
    /* PENDING update always takes priority —
     * SBL_SystemReboot writes RESTART after PENDING with a higher
     * sequence, so pure sequence-order would shadow the update. */
    if (r->txn_state == TOS_TXN_STATE_PENDING) return r;
    if (!latest || r->sequence >= latest->sequence) latest = r;
  }
  return latest;
}

static uint8_t ELF_ManifestPresent(void) {
  /* State area lives in ELF's own last 4 KB — always present. */
  return 1U;
}

static uint8_t ELF_SignedImageValid(uint32_t storage_address,
                                      uint32_t load_address,
                                      uint32_t size,
                                      uint32_t type) {
  return SecureBoot_Verify(storage_address, load_address, size, type, 1U) ==
                 SECUREBOOT_VERIFY_OK
             ? 1U
             : 0U;
}

static uint8_t ELF_VectorValid(uint32_t address, uint32_t size) {
  const uint32_t *v = (const uint32_t *)address;
  uint32_t sp = v[0];
  uint32_t pc = v[1];
  if (sp < 0x20000000UL || sp > 0x20020000UL || (sp & 7U) != 0U) return 0U;
  if ((pc & 1U) == 0U) return 0U;
  pc &= ~1UL;
  return pc >= address && pc < address + size ? 1U : 0U;
}

static uint8_t ELF_TransactionBounds(const TosTeeStateRecord_t *r) {
  if (r->source_address != TOS_TMP_STAGE_ADDRESS || r->image_size == 0U ||
      r->image_size > TOS_TMP_STAGE_SIZE) return 0U;
  if (r->update_kind == TOS_UPDATE_SBL) {
    return r->target_address == TOS_PART_SBL_ADDRESS &&
           r->image_size <= TOS_PART_SBL_SIZE ? 1U : 0U;
  }
  return 0U;
}

static uint8_t ELF_SetRestart(void) {
  const TosTeeStateRecord_t *latest = ELF_LatestState();
  TosTeeStateRecord_t r;
  const uint32_t *w;
  uint32_t address = 0U;
  uint32_t off;

  for (off = 0U; off + sizeof(r) <= TOS_TEE_STATE_SIZE;
       off += sizeof(r)) {
    const uint32_t *p = (const uint32_t *)(TOS_TEE_STATE_ADDRESS + off);
    if (p[0] == 0xFFFFFFFFUL) { address = TOS_TEE_STATE_ADDRESS + off; break; }
  }
  if (!address) return 0U;

  r.magic           = TOS_TEE_STATE_MAGIC;
  r.version         = TOS_TEE_STATE_VERSION;
  r.sequence        = latest ? latest->sequence + 1U : 1U;
  r.unlocked        = latest ? latest->unlocked : 0U;
  r.boot_target     = TOS_BOOT_TARGET_NONE;
  r.update_kind     = TOS_UPDATE_NONE;
  r.txn_state       = TOS_TXN_STATE_RESTART;
  r.source_address  = 0xFFFFFFFFUL;
  r.target_address  = 0xFFFFFFFFUL;
  r.image_size      = 0U;
  r.image_crc32     = 0xFFFFFFFFUL;
  r.post_boot_target= TOS_BOOT_TARGET_NONE;
  r.reserved0       = 0xFFFFFFFFUL;
  r.reserved1       = 0xFFFFFFFFUL;
  r.reserved2       = 0xFFFFFFFFUL;
  r.record_crc      = TosTeeStateRecordCrc(&r);

  if (!ELF_FlashUnlock()) return 0U;
  w = (const uint32_t *)&r;
  for (uint32_t i = 0U; i < sizeof(r) / sizeof(uint32_t); ++i) {
    if (!ELF_FlashProgramWord(address + i * 4U, w[i])) {
      ELF_FlashLock();
      return 0U;
    }
  }
  ELF_FlashLock();
  ELF_FlashFlush();
  return 1U;
}

static uint8_t ELF_MarkState(const TosTeeStateRecord_t *r, uint32_t state) {
  uint32_t address = (uint32_t)(uintptr_t)&r->txn_state;
  uint8_t ok = 0U;
  if (!ELF_FlashUnlock()) return 0U;
  ok = ELF_FlashProgramWord(address, state);
  ELF_FlashLock();
  ELF_FlashFlush();
  return ok;
}

static uint8_t ELF_ApplyUpdate(const TosTeeStateRecord_t *r) {
  const uint32_t *src = (const uint32_t *)r->source_address;
  uint32_t words = r->image_size / 4U;

  if (!ELF_TransactionBounds(r) || !ELF_ManifestPresent() ||
      ELF_Crc32(src, r->image_size) != r->image_crc32 ||
      !ELF_SignedImageValid(r->source_address, r->target_address,
                              TOS_PART_SBL_SIZE, TOS_IMAGE_TYPE_SBL)) {
    (void)ELF_MarkState(r, TOS_TXN_STATE_FAILED);
    return 0U;
  }

  if (!ELF_FlashUnlock()) return 0U;
  /* SBL spans all three 16-KiB sectors at 0x08004000..0x0800FFFF. */
  if (!ELF_FlashEraseSector(1U) || !ELF_FlashEraseSector(2U) ||
      !ELF_FlashEraseSector(3U)) {
    ELF_FlashLock();
    return 0U;
  }

  for (uint32_t i = 0U; i < words; ++i) {
    if (!ELF_FlashProgramWord(r->target_address + i * 4U, src[i])) {
      ELF_FlashLock();
      return 0U;
    }
  }
  ELF_FlashLock();
  ELF_FlashFlush();

  if (ELF_Crc32((const void *)(uintptr_t)r->target_address, r->image_size) !=
          r->image_crc32 ||
      !ELF_SignedImageValid(r->target_address, r->target_address,
                              TOS_PART_SBL_SIZE, TOS_IMAGE_TYPE_SBL)) {
    return 0U;
  }
  return ELF_MarkState(r, TOS_TXN_STATE_DONE);
}

static void ELF_Reset(void) {
  /* Persist a one-shot restart marker before requesting a system reset. */
  (void)ELF_SetRestart();
  __disable_irq();
  __DSB();
  SCB->AIRCR = (0x5FAUL << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
  __DSB();
  while (1) __NOP();
}

static void ELF_Jump(uint32_t address) {
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
  const TosTeeStateRecord_t *state;
  SCB->VTOR = TOS_PART_ELF_ADDRESS;
  Cust_Setup();
  state = ELF_LatestState();
  /* A restart record is only a one-shot UI hint. It must never weaken the
   * root-of-trust check: SYSTEM can request a reset and may be compromised.
   * Consume it with a legal 1->0 flash transition; writing 0xFFFFFFFF cannot
   * erase a programmed word and used to leave this path active forever. */
  if (state && state->txn_state == TOS_TXN_STATE_RESTART &&
      ELF_SignedImageValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_ADDRESS,
                             TOS_PART_SBL_SIZE, TOS_IMAGE_TYPE_SBL) &&
      ELF_VectorValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_SIZE)) {
    if (state->boot_target == TOS_BOOT_TARGET_NONE)
      (void)ELF_MarkState(state, ELF_TXN_STATE_CONSUMED);
    ELF_Jump(TOS_PART_SBL_ADDRESS);
  }
  if (state && state->txn_state == TOS_TXN_STATE_PENDING &&
      state->update_kind != TOS_UPDATE_NONE) {
    if (ELF_ApplyUpdate(state)) ELF_Reset();
    if (state->txn_state == TOS_TXN_STATE_FAILED &&
        ELF_SignedImageValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_ADDRESS,
                               TOS_PART_SBL_SIZE, TOS_IMAGE_TYPE_SBL) &&
        ELF_VectorValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_SIZE)) {
      ELF_Jump(TOS_PART_SBL_ADDRESS);
    }
    while (1) __NOP();
  }
  if (ELF_SignedImageValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_ADDRESS,
                             TOS_PART_SBL_SIZE, TOS_IMAGE_TYPE_SBL) &&
      ELF_VectorValid(TOS_PART_SBL_ADDRESS, TOS_PART_SBL_SIZE)) {
    Cust_Finally();
    ELF_Jump(TOS_PART_SBL_ADDRESS);
  }
  while (1) __NOP();
}
