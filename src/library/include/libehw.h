/**
 ******************************************************************************
 * @file    libehw.h
 * @author  Typheye
 * @brief   Libehw interface.
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

#ifndef LIBEHW_H
#define LIBEHW_H

#include <stdint.h>
#ifdef __cplusplus
#include "hardware/include/jy901s.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/bmp180.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/tcs3472.hpp"
#endif
#include <math.h>
#include <stdio.h>
#include "core/sys/include/syslog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EHW_EXPR_NONE = 0,
  EHW_EXPR_DIZZY,    // JY901S: violent shake
  EHW_EXPR_PETTED,   // JY901S: gentle movement
  EHW_EXPR_COLD,     // BMP180: < 15°C
  EHW_EXPR_COMFY,    // BMP180: 22~28°C
  EHW_EXPR_HOT,      // BMP180: > 35°C
  EHW_EXPR_DARK,     // TCS3472: < 10 lux
  EHW_EXPR_BRIGHT,   // TCS3472: > 500 lux
} EHW_Expr_t;

void  EHW_Init(void);
EHW_Expr_t EHW_Update(void);       // poll sensors, return dominant expression
EHW_Expr_t EHW_GetExpr(void);      // get last expression without re-polling

#ifdef __cplusplus
}
#endif

#endif
