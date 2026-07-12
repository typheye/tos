/**
 ******************************************************************************
 * @file    settings.hpp
 * @author  Typheye
 * @brief   Settings interface.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
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
