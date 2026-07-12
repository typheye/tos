/**
 ******************************************************************************
 * @file    keyboard.hpp
 * @author  Typheye
 * @brief   Keyboard interface.
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

#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/pot.hpp"
#include <cstdio>
#include <cstring>
#include "core/sys/include/syslog.h"

// Opens an ASCII keyboard overlay for password entry.
// title: shown at top (e.g. "WiFi Password")
// max_len: max password length (1~24)
// out: buffer to receive the password (caller-allocated, must be >= max_len+1)
// Returns: true if user confirmed (pressed Connect), false if cancelled.
bool keyboard_open(const char *title, char *out, int max_len);

#endif
