/**
 ******************************************************************************
 * @file    launcher.hpp
 * @author  Typheye
 * @brief   Launcher interface.
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

#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "core/manager/include/emotion_manager.h"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "library/include/libehw.h"
#include "library/include/libemo.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "core/sys/include/syslog.h"

// Run the pet launcher loop. Returns when user long-presses Enter.
void petLauncherRun(void);

#endif
