/**
 ******************************************************************************
 * @file    build.h
 * @author  Typheye
 * @brief   SBL build-time constants interface.
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

#ifndef SBL_BUILD_H
#define SBL_BUILD_H

#include "manifest.h"

#define SBL_BUILD_PRODUCT_NAME          TOS_PRODUCT_NAME
#define SBL_BUILD_VERSION               TOS_VERSION
#define SBL_BUILD_VERSION_BOOTLOADER    TOS_BL_VERSION
#define SBL_BUILD_VERSION_BASEBAND      TOS_BB_VERSION

#endif /* SBL_BUILD_H */
