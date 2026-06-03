/**
 ******************************************************************************
 * @file    libemo.h
 * @author  Typheye
 * @brief   Libemo interface.
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

#ifndef __LIBEMO_H
#define __LIBEMO_H

#include <stdint.h>
#include "include/lcd.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EMO_WHITE   0xFFFFFF
#define EMO_BLACK   0x000000
#define EMO_GREY    0x444444
#define EMO_BROW    0x666666  // darker grey for eyebrows
#define EMO_MOUTH   0xCCDDFF  // soft white-blue for mouth

// Face layout
#define EMO_EYE_R        20
#define EMO_PUPIL_R      7
#define EMO_LEFT_EYE_X   82
#define EMO_LEFT_EYE_Y   78
#define EMO_RIGHT_EYE_X  158
#define EMO_RIGHT_EYE_Y  78
#define EMO_MOUTH_CX     120
#define EMO_MOUTH_CY     152
#define EMO_MOUTH_R      35

void EMO_Init(void);
void EMO_SetTileWindow(uint16_t y, uint16_t h);
void EMO_FillScreen(uint32_t color);
void EMO_FillCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color);
void EMO_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color);
void EMO_DrawHLine(int16_t x, int16_t y, int16_t len, int16_t t, uint32_t color);
void EMO_DrawThickArc(int16_t cx, int16_t cy, int16_t r, float s_deg, float e_deg, int16_t t, uint32_t color);
void EMO_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color);

// blink_l / blink_r : 0=open, 1=closed (independent per eye for wink)
// mouth_open: 0=neutral arc, 0.5=wide smile, 1.0=round mouth
// look_x/y: -1..1 pupil direction
// cheek: 0..1 blush amount
// brow_y: -1=lowered, 0=neutral, +1=raised
void EMO_DrawFace(float blink_l, float blink_r, float mouth_open,
                  float look_x, float look_y, float cheek, float brow_y);

#ifdef __cplusplus
}
#endif

#endif
