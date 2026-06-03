/**
 ******************************************************************************
 * @file    hotspot_activity.hpp
 * @author  Typheye
 * @brief   Hotspot Activity interface.
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

#ifndef HOTSPOT_ACTIVITY_HPP
#define HOTSPOT_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/manager/include/settings_manager.h"
#include "core/sys/include/systime.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"
#include <cstdio>
#include <cstring>

void hotspot_activity_run(void);
#endif
