/**
 ******************************************************************************
 * @file    splash.h
 * @author  Typheye
 * @brief   SBL splash screen interface.
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
#ifndef SBL_SPLASH_H
#define SBL_SPLASH_H

#include "common.h"
#include "manifest.h"

#if LCD_ENABLED
SBL_CODE void SBL_SplashRun(void);
#else
#define SBL_SplashRun() do{}while(0)
#endif

#endif /* SBL_SPLASH_H */
