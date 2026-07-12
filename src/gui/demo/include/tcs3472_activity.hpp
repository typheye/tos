/**
 ******************************************************************************
 * @file    tcs3472_activity.hpp
 * @author  Typheye
 * @brief   Tcs3472 Activity interface.
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

#ifndef TCS3472_ACTIVITY_HPP
#define TCS3472_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/tcs3472.hpp"
#include "core/sys/include/syslog.h"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


void tcs3472_activity(void);


void tcs3472_color_demo(void);


void tcs3472_cct_demo(void);


void tcs3472_read_activity(void);


void tcs3472_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* TCS3472_ACTIVITY_HPP */