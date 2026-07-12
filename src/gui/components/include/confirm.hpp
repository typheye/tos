/**
 ******************************************************************************
 * @file    confirm.hpp
 * @author  Typheye
 * @brief   Confirm interface.
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

#ifndef CONFIRM_HPP
#define CONFIRM_HPP
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Show a Yes/No confirmation dialog
 * @param  title  Dialog title
 * @param  msg    Message text
 * @return true if Yes selected, false if No selected
 */
bool confirm_show(const char *title, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
