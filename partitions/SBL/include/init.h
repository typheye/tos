/**
 ******************************************************************************
 * @file    init.h
 * @author  Typheye
 * @brief   SBL boot initialization interface.
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

#ifndef SBL_INIT_H
#define SBL_INIT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void SBL_Main(void);
void SBL_Run(void);
uint8_t SBL_AppLooksValid(void);
#ifdef __cplusplus
}
#endif
#endif
