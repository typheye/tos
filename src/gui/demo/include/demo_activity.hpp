/**
 ******************************************************************************
 * @file    demo_activity.hpp
 * @author  Typheye
 * @brief   Demo Activity interface.
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
