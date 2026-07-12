/**
 ******************************************************************************
 * @file    i2c_activity.hpp
 * @author  Typheye
 * @brief   I2C Activity interface.
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

#ifndef I2C_ACTIVITY_HPP
#define I2C_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "main.h"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

void i2c_scan_activity(void);
void i2c_scan_activity_gui(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_ACTIVITY_HPP