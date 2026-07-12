/**
 ******************************************************************************
 * @file    wlan_activity.hpp
 * @author  Typheye
 * @brief   Wlan Activity interface.
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

#ifndef WLAN_ACTIVITY_HPP
#define WLAN_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/manager/include/settings_manager.h"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#include "core/sys/include/syswatchdog.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"
#include <cstdio>
#include <cstring>

void wlan_activity_run(void);

#endif
