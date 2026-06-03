/**
 ******************************************************************************
 * @file    libehw.h
 * @author  Typheye
 * @brief   Libehw interface.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef __LIBEHW_H
#define __LIBEHW_H

#include <stdint.h>

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
