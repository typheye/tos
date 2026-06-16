#include "sbl_common.h"

static volatile uint32_t sbl_tick_ms;

SBL_CODE void HAL_IncTick(void) {
  sbl_tick_ms++;
}

SBL_CODE uint32_t HAL_GetTick(void) {
  return sbl_tick_ms;
}

SBL_CODE void HAL_Delay(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < ms) {
    __NOP();
  }
}

#define SBL_FLASH_STATUS_ERRORS \
  (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
   FLASH_SR_PGPERR | FLASH_SR_PGSERR)

static SBL_CODE uint8_t sbl_flash_wait_ready(void) {
  uint32_t guard = 0x00FFFFFFUL;
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    if (--guard == 0U) {
      return 0U;
    }
  }
  return 1U;
}

SBL_CODE void SBL_Delay(volatile uint32_t loops) {
  while (loops--) {
    __NOP();
  }
}

SBL_CODE void SBL_DelayMs(uint32_t ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < ms) {
    __NOP();
  }
}

SBL_CODE void SBL_GpioSet(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << pin);
}

SBL_CODE void SBL_GpioReset(GPIO_TypeDef *port, uint32_t pin) {
  port->BSRR = (1UL << (pin + 16U));
}

SBL_CODE uint8_t SBL_FlashUnlock(void) {
  if (!sbl_flash_wait_ready()) {
    return 0U;
  }
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
    FLASH->KEYR = 0x45670123UL;
    FLASH->KEYR = 0xCDEF89ABUL;
  }
  return ((FLASH->CR & FLASH_CR_LOCK) == 0U) ? 1U : 0U;
}

SBL_CODE void SBL_FlashLock(void) {
  FLASH->CR |= FLASH_CR_LOCK;
}

SBL_CODE void SBL_FlashClearStatus(void) {
  FLASH->SR = FLASH_SR_EOP | SBL_FLASH_STATUS_ERRORS;
}

SBL_CODE uint8_t SBL_FlashProgramWord(uint32_t addr, uint32_t word) {
  if (!sbl_flash_wait_ready()) {
    return 0U;
  }

  SBL_FlashClearStatus();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_PG);
  FLASH->CR |= FLASH_CR_PSIZE_1;
  FLASH->CR |= FLASH_CR_PG;

  *(volatile uint32_t *)addr = word;

  if (!sbl_flash_wait_ready()) {
    FLASH->CR &= ~FLASH_CR_PG;
    return 0U;
  }

  FLASH->CR &= ~FLASH_CR_PG;
  if ((FLASH->SR & SBL_FLASH_STATUS_ERRORS) != 0U) {
    SBL_FlashClearStatus();
    return 0U;
  }
  return 1U;
}

SBL_CODE uint8_t SBL_FlashEraseSectorIndex(uint32_t sector_index) {
  if (!sbl_flash_wait_ready()) {
    return 0U;
  }

  SBL_FlashClearStatus();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_SER | FLASH_CR_MER);
  FLASH->CR |= FLASH_CR_PSIZE_1;
  FLASH->CR |= FLASH_CR_SER;
  FLASH->CR |= ((sector_index << FLASH_CR_SNB_Pos) & FLASH_CR_SNB);
  FLASH->CR |= FLASH_CR_STRT;

  if (!sbl_flash_wait_ready()) {
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0U;
  }

  FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
  if ((FLASH->SR & SBL_FLASH_STATUS_ERRORS) != 0U) {
    SBL_FlashClearStatus();
    return 0U;
  }
  return 1U;
}
