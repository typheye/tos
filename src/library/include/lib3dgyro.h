/**
 ******************************************************************************
 * @file    lib3dgyro.h
 * @author  Typheye
 * @brief   Lib3Dgyro interface.
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

#ifndef LIB3DGYRO_H
#define LIB3DGYRO_H

#include <stdint.h>
#include "library/include/libpd.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

void gyro_cube_init(int16_t center_x, int16_t center_y, int16_t size);
void gyro_cube_draw(float roll, float pitch, float yaw);

#ifdef __cplusplus
}
#endif

#endif