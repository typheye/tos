/**
 ******************************************************************************
 * @file    demo_activity.hpp
 * @author  Typheye
 * @brief   Demo Activity interface.
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

#ifndef DEMO_ACTIVITY_HPP
#define DEMO_ACTIVITY_HPP
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#include "gui/include/settings.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "gui/demo/include/3dox_activity.hpp"
#include "gui/demo/include/bmp_activity.hpp"
#include "gui/demo/include/i2c_activity.hpp"
#include "gui/demo/include/jyro_activity.hpp"
#include "gui/demo/include/key_activity.hpp"
#include "include/libpd.h"
#include "gui/demo/include/sn74hc00n_activity.hpp"
#include "gui/demo/include/tcs3472_activity.hpp"
#include <cstdio>

int demo_activity_run(void); // TOS menu, returns 1 if user chose Return
void demo_list_run(void);    // Full demo list (called from Debugs)

#endif
