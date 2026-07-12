/**
 ******************************************************************************
 * @file    cust.h
 * @author  Typheye
 * @brief   ELF board-specific customization interface.
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

#ifndef CUST_H
#define CUST_H

#include "stm32f4xx.h"

void Cust_Setup(void);
void Cust_Init(void);
void Cust_Finally(void);

#endif /* CUST_H */
