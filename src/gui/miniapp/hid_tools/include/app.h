/**
 ******************************************************************************
 * @file    app.h
 * @author  Typheye
 * @brief   App interface.
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

#ifndef HID_TOOLS_APP_H
#define HID_TOOLS_APP_H
#ifdef __cplusplus
#include "gui/miniapp/hid_tools/include/hid_tools_pages.hpp"
#endif
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#ifdef __cplusplus
#include "hardware/include/key.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/lcd.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/trtc.hpp"
#endif
#include "include/libpd.h"

#ifdef __cplusplus
extern "C" {
#endif

void hid_tools_run(void);

#ifdef __cplusplus
}
#endif

#endif
