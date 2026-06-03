/**
 ******************************************************************************
 * @file    display_activity.hpp
 * @author  Typheye
 * @brief   Display Activity interface.
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

#ifndef DISPLAY_ACTIVITY_HPP
#define DISPLAY_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "core/manager/include/settings_manager.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>
#include "core/sys/include/syslog.h"
#include "core/sys/include/systime.h"

void display_activity_run(void);

#endif
