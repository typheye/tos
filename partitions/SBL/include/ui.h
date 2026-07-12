/**
 ******************************************************************************
 * @file    ui.h
 * @author  Typheye
 * @brief   SBL fastboot UI and recovery exception interface.
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
#ifndef SBL_UI_H
#define SBL_UI_H

#include "common.h"
#include "secure_boot.h"

SBL_CODE void SBL_UiDrawFastboot(void);
SBL_CODE void SBL_UiRunFastboot(void);
SBL_CODE void SBL_UiRunSystemDamage(SecureBoot_Result_t reason);
SBL_CODE void SBL_UiRunRecoveryException(void);

#endif /* SBL_UI_H */
