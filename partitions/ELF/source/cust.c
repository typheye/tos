/**
 ******************************************************************************
 * @file    cust.c
 * @author  Typheye
 * @brief   ELF board-specific customization implementation.
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

#include "manifest.h"
#include "stm32f4xx.h"

/* GPIO bootstrap — runs before any verification or TEE reads.
 * Hold external hardware in safe state until SBL takes over. */
void Cust_Setup(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                  RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOFEN;
  (void)RCC->AHB1ENR;

  /* BTN: input + pull-up */
  BTN_PORT->MODER &= ~(3UL << (BTN_PIN * 2U));
  BTN_PORT->PUPDR &= ~(3UL << (BTN_PIN * 2U));
  BTN_PORT->PUPDR |=  (1UL << (BTN_PIN * 2U));

  /* LCD RST high, BL off */
  GPIOD->MODER &= ~((3UL << (14U * 2U)) | (3UL << (13U * 2U)));
  GPIOD->MODER |=  ((1UL << (14U * 2U)) | (1UL << (13U * 2U)));
  GPIOD->BSRR = (1UL << 14U) | (1UL << (13U + 16U));

  /* PF10 out low */
  GPIOF->MODER &= ~(3UL << (10U * 2U));
  GPIOF->MODER |=  (1UL << (10U * 2U));
  GPIOF->BSRR = (1UL << (10U + 16U));
}

void Cust_Init(void)   { /* reserved */ }
void Cust_Finally(void) { /* reserved */ }
