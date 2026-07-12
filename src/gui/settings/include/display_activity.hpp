/**
 ******************************************************************************
 * @file    display_activity.hpp
 * @author  Typheye
 * @brief   Display Activity interface.
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
