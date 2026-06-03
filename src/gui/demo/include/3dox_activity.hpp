/**
 ******************************************************************************
 * @file    3dox_activity.hpp
 * @author  Typheye
 * @brief   3Dox Activity interface.
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

#ifndef _3DOX_ACTIVITY_HPP
#define _3DOX_ACTIVITY_HPP
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/lib3dox.h"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void render_3dox_activity(void);
void render_3dox_activity_with_exit(void);

#ifdef __cplusplus
}
#endif

#endif // _3DOX_ACTIVITY_HPP