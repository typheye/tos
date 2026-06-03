/**
 ******************************************************************************
 * @file    settings.hpp
 * @author  Typheye
 * @brief   Settings interface.
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

#ifndef SETTINGS_HPP
#define SETTINGS_HPP
#include "core/sdk/include/tos_api.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "gui/settings/include/hotspot_activity.hpp"
#include "gui/settings/include/wlan_activity.hpp"
#include "gui/settings/include/storage_activity.hpp"
#include "gui/settings/include/display_activity.hpp"
#include "gui/settings/include/sound_activity.hpp"
#include "gui/settings/include/time_activity.hpp"
#include "gui/settings/include/about_activity.hpp"
#include <cstdio>
#include "core/sys/include/syslog.h"
#include "core/sys/include/systime.h"

void settings_run(void);

#endif
