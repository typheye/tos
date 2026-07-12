/**
 ******************************************************************************
 * @file    jyro_activity.hpp
 * @author  Typheye
 * @brief   Jyro Activity interface.
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

#ifndef JYRO_ACTIVITY_HPP
#define JYRO_ACTIVITY_HPP
#include "core/sys/include/systime.h"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include "core/sys/include/syslog.h"
#include <cstdint>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif


void jyro_activity(void);


void jyro_cube_activity(void);


void jyro_text_activity(void);


void jyro_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* JYRO_ACTIVITY_HPP */