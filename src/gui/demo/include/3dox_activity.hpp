/**
 ******************************************************************************
 * @file    3dox_activity.hpp
 * @author  Typheye
 * @brief   3Dox Activity interface.
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

#ifndef _3DOX_ACTIVITY_HPP
#define _3DOX_ACTIVITY_HPP
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/lib3dox.h"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void render_3dox_activity(void);
void render_3dox_activity_with_exit(void);

#ifdef __cplusplus
}
#endif

#endif // _3DOX_ACTIVITY_HPP