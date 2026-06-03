/**
 ******************************************************************************
 * @file    about_activity.hpp
 * @author  Typheye
 * @brief   About Activity interface.
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

#ifndef ABOUT_ACTIVITY_HPP
#define ABOUT_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "components/include/confirm.hpp"
#include "core/include/config.h"
#include "core/manager/include/settings_manager.h"
#include "core/sys/include/systime.h"
#include "core/sdk/include/tos_api.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/sfhd.h"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

void about_activity_run(void);

#ifdef __cplusplus
}
#endif

#endif
