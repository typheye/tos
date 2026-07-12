/**
 ******************************************************************************
 * @file    libvan.h
 * @author  Typheye
 * @brief   Libvan interface.
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

#ifndef LIBVAN_H
#define LIBVAN_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void float_to_str(float value, char *str);
void float_to_str_precision(float value, char *str, int precision);
void float_to_str_signed(float value, char *str);

#ifdef __cplusplus
}
#endif

#endif /* LIBVAN_H */
