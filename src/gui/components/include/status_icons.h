/**
 ******************************************************************************
 * @file    status_icons.h
 * @author  Typheye
 * @brief   Status Icons interface.
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

#ifndef STATUS_ICONS_H
#define STATUS_ICONS_H

#include <stdint.h>
#include <stdbool.h>
#include "include/libpd.h"
#include "gui/components/include/icons.h"

#ifdef __cplusplus
extern "C" {
#endif

void draw_icon_signal(int x, int y, int signal);
void draw_icon_wifi(int x, int y, bool on);
void draw_icon_ico(void);
void status_icons_draw(bool wlan_on, bool wlan_connected, bool hotspot_on);

#ifdef __cplusplus
}
#endif

#endif
