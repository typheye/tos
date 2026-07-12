/**
 ******************************************************************************
 * @file    common.c
 * @author  Typheye
 * @brief   SBL common utilities (flash, delay, GPIO) implementation.
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

#include "common.h"

#include <stddef.h>

static volatile uint32_t sbl_tick_ms;

/*
 * Keep the compiler's ARM EABI memory helpers inside SBL.  Aggregate
 * initializers in SBL/REC may be lowered to these symbols even when the
 * source does not call memset/memcpy directly.  Volatile byte accesses keep
 * the helpers self-contained instead of being optimized back into a runtime
 * library call that could otherwise be linked from SYSTEM.
 */
static SBL_CODE void sbl_mem_fill_volatile(void *dst, uint8_t value,
                                           size_t len) {
  volatile uint8_t *p = (volatile uint8_t *)dst;
  while (len-- != 0U) {
    *p++ = value;
  }
}

static SBL_CODE void sbl_mem_copy_volatile(void *dst, const void *src,
                                           size_t len) {
  volatile uint8_t *d = (volatile uint8_t *)dst;
  const volatile uint8_t *s = (const volatile uint8_t *)src;
  while (len-- != 0U) {
    *d++ = *s++;
  }
}

static SBL_CODE void sbl_mem_move_volatile(void *dst, const void *src,
                                           size_t len) {
  volatile uint8_t *d = (volatile uint8_t *)dst;
  const volatile uint8_t *s = (const volatile uint8_t *)src;

  if (d > s && d < (s + len)) {
    d += len;
    s += len;
    while (len-- != 0U) {
      *--d = *--s;
    }
  } else {
    while (len-- != 0U) {
      *d++ = *s++;
    }
  }
}

SBL_CODE void __aeabi_memclr(void *dst, size_t len) {
  sbl_mem_fill_volatile(dst, 0U, len);
}

SBL_CODE void __aeabi_memclr4(void *dst, size_t len) {
  sbl_mem_fill_volatile(dst, 0U, len);
}

SBL_CODE void __aeabi_memclr8(void *dst, size_t len) {
  sbl_mem_fill_volatile(dst, 0U, len);
}

SBL_CODE void __aeabi_memset(void *dst, size_t len, int value) {
  sbl_mem_fill_volatile(dst, (uint8_t)value, len);
}

SBL_CODE void __aeabi_memset4(void *dst, size_t len, int value) {
  sbl_mem_fill_volatile(dst, (uint8_t)value, len);
}

SBL_CODE void __aeabi_memset8(void *dst, size_t len, int value) {
  sbl_mem_fill_volatile(dst, (uint8_t)value, len);
}

SBL_CODE void __aeabi_memcpy(void *dst, const void *src, size_t len) {
  sbl_mem_copy_volatile(dst, src, len);
}

SBL_CODE void __aeabi_memcpy4(void *dst, const void *src, size_t len) {
  sbl_mem_copy_volatile(dst, src, len);
}

SBL_CODE void __aeabi_memcpy8(void *dst, const void *src, size_t len) {
  sbl_mem_copy_volatile(dst, src, len);
}

SBL_CODE void __aeabi_memmove(void *dst, const void *src, size_t len) {
  sbl_mem_move_volatile(dst, src, len);
}

SBL_CODE void __aeabi_memmove4(void *dst, const void *src, size_t len) {
  sbl_mem_move_volatile(dst, src, len);
}

SBL_CODE void __aeabi_memmove8(void *dst, const void *src, size_t len) {
  sbl_mem_move_volatile(dst, src, len);
}

SBL_CODE void *memset(void *dst, int value, size_t len) {
  uint8_t *p = (uint8_t *)dst;
  while (len--) {
    *p++ = (uint8_t)value;
  }
  return dst;
}

SBL_CODE void *memcpy(void *dst, const void *src, size_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  while (len--) {
    *d++ = *s++;
  }
  return dst;
}

SBL_CODE void *memmove(void *dst, const void *src, size_t len) {
  uint8_t *d = (uint8_t *)dst;
  const uint8_t *s = (const uint8_t *)src;
  if (d > s && d < (s + len)) {
    d += len;
    s += len;
    while (len--) {
      *--d = *--s;
    }
  } else {
    while (len--) {
      *d++ = *s++;
    }
  }
  return dst;
}

SBL_CODE int memcmp(const void *a, const void *b, size_t len) {
  const uint8_t *pa = (const uint8_t *)a;
  const uint8_t *pb = (const uint8_t *)b;
  while (len--) {
    if (*pa != *pb) {
      return (int)*pa - (int)*pb;
    }
    pa++;
    pb++;
  }
  return 0;
}

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

static SBL_CODE uint8_t sbl_flash_wait_ready(uint32_t guard) {
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
  if (!sbl_flash_wait_ready(0x0FFFFFFFUL)) {
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

SBL_CODE void SBL_FlashFlushCaches(void) {
  uint32_t acr = FLASH->ACR;

  if ((acr & FLASH_ACR_ICEN) != 0U) {
    FLASH->ACR = acr & ~FLASH_ACR_ICEN;
    FLASH->ACR |= FLASH_ACR_ICRST;
    FLASH->ACR &= ~FLASH_ACR_ICRST;
    FLASH->ACR |= FLASH_ACR_ICEN;
  }

  acr = FLASH->ACR;
  if ((acr & FLASH_ACR_DCEN) != 0U) {
    FLASH->ACR = acr & ~FLASH_ACR_DCEN;
    FLASH->ACR |= FLASH_ACR_DCRST;
    FLASH->ACR &= ~FLASH_ACR_DCRST;
    FLASH->ACR |= FLASH_ACR_DCEN;
  }
}

SBL_CODE uint8_t SBL_FlashProgramWord(uint32_t addr, uint32_t word) {
  if (!sbl_flash_wait_ready(0x0FFFFFFFUL)) {
    return 0U;
  }

  SBL_FlashClearStatus();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SER | FLASH_CR_SNB | FLASH_CR_PG);
  FLASH->CR |= FLASH_CR_PSIZE_1;
  FLASH->CR |= FLASH_CR_PG;

  *(volatile uint32_t *)addr = word;

  if (!sbl_flash_wait_ready(0x0FFFFFFFUL)) {
    FLASH->CR &= ~FLASH_CR_PG;
    return 0U;
  }

  FLASH->CR &= ~FLASH_CR_PG;
  if ((FLASH->SR & SBL_FLASH_STATUS_ERRORS) != 0U) {
    SBL_FlashClearStatus();
    return 0U;
  }
  SBL_FlashFlushCaches();
  return 1U;
}

SBL_CODE uint8_t SBL_FlashEraseSectorIndex(uint32_t sector_index) {
  if (!sbl_flash_wait_ready(0x0FFFFFFFUL)) {
    return 0U;
  }

  SBL_FlashClearStatus();
  FLASH->CR &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB | FLASH_CR_SER | FLASH_CR_MER);
  FLASH->CR |= FLASH_CR_PSIZE_1;
  FLASH->CR |= FLASH_CR_SER;
  FLASH->CR |= ((sector_index << FLASH_CR_SNB_Pos) & FLASH_CR_SNB);
  FLASH->CR |= FLASH_CR_STRT;

  if (!sbl_flash_wait_ready(0xFFFFFFFFUL)) {
    FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0U;
  }

  FLASH->CR &= ~(FLASH_CR_SER | FLASH_CR_SNB);
  if ((FLASH->SR & SBL_FLASH_STATUS_ERRORS) != 0U) {
    SBL_FlashClearStatus();
    return 0U;
  }
  SBL_FlashFlushCaches();
  return 1U;
}
