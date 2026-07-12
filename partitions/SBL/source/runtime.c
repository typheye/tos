/**
 ******************************************************************************
 * @file    runtime.c
 * @author  Typheye
 * @brief   SBL runtime entry implementation.
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

#include "stm32f4xx_hal.h"
#include "usbd_conf.h"

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

void SysTick_Handler(void) { HAL_IncTick(); }
void OTG_FS_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS); }
void Error_Handler(void) { __disable_irq(); while (1) { __NOP(); } }
