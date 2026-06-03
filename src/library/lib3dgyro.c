/**
 ******************************************************************************
 * @file    lib3dgyro.c
 * @author  Typheye
 * @brief   Lib3Dgyro implementation.
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

#include "include/lib3dgyro.h"


#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

static const float cube_vertices[8][3] = {
    {-50, -50, -50}, {50, -50, -50}, {50, -50, 50}, {-50, -50, 50},
    {-50, 50, -50},  {50, 50, -50},  {50, 50, 50},  {-50, 50, 50}};

static const uint8_t cube_edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},
                                          {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                          {0, 4}, {1, 5}, {2, 6}, {3, 7}};

static CCMRAM int16_t g_center_x = 120;
static CCMRAM int16_t g_center_y = 120;
static CCMRAM int16_t g_cube_size = 80;


static CCMRAM int16_t last_proj_x[8] = {0};
static CCMRAM int16_t last_proj_y[8] = {0};
static CCMRAM int16_t last_edges[12][4] = {0};
static CCMRAM uint8_t first_frame = 1;

static void rotate_point(float *x, float *y, float *z, float roll, float pitch,
                         float yaw) {
  float tx, ty, tz;
  // Yaw
  tx = *x * cosf(yaw) - *y * sinf(yaw);
  ty = *x * sinf(yaw) + *y * cosf(yaw);
  tz = *z;
  *x = tx;
  *y = ty;
  *z = tz;
  // Pitch
  tx = *x * cosf(pitch) + *z * sinf(pitch);
  tz = -*x * sinf(pitch) + *z * cosf(pitch);
  *x = tx;
  *z = tz;
  // Roll
  ty = *y * cosf(roll) - *z * sinf(roll);
  tz = *y * sinf(roll) + *z * cosf(roll);
  *y = ty;
  *z = tz;
}

static void project(float x, float y, float z, int16_t *px, int16_t *py) {
  float scale = 1.0f / (1.0f + z / 200.0f);
  *px = g_center_x + (int16_t)(x * scale);
  *py = g_center_y + (int16_t)(y * scale);
}

void gyro_cube_init(int16_t center_x, int16_t center_y, int16_t size) {
  g_center_x = center_x;
  g_center_y = center_y;
  g_cube_size = size;
  first_frame = 1;
}

void gyro_cube_draw(float roll, float pitch, float yaw) {
  float rotated[8][3];
  int16_t proj_x[8], proj_y[8];
  float scale = g_cube_size / 100.0f;

  
  for (int i = 0; i < 8; i++) {
    rotated[i][0] = cube_vertices[i][0] * scale;
    rotated[i][1] = cube_vertices[i][1] * scale;
    rotated[i][2] = cube_vertices[i][2] * scale;
    rotate_point(&rotated[i][0], &rotated[i][1], &rotated[i][2], roll, pitch,
                 yaw);
    project(rotated[i][0], rotated[i][1], rotated[i][2], &proj_x[i],
            &proj_y[i]);
  }

  if (!first_frame) {
    
    PD_SetColor(LCD_COLOR_BLACK);
    for (int i = 0; i < 12; i++) {
      PD_DrawLine(last_edges[i][0], last_edges[i][1], last_edges[i][2],
                  last_edges[i][3]);
    }
    
    for (int i = 0; i < 8; i++) {
      PD_DrawCircle(last_proj_x[i], last_proj_y[i], 4);
    }
  }

  
  PD_SetColor(LCD_COLOR_WHITE);
  for (int i = 0; i < 12; i++) {
    int idx1 = cube_edges[i][0];
    int idx2 = cube_edges[i][1];
    PD_DrawLine(proj_x[idx1], proj_y[idx1], proj_x[idx2], proj_y[idx2]);
    last_edges[i][0] = proj_x[idx1];
    last_edges[i][1] = proj_y[idx1];
    last_edges[i][2] = proj_x[idx2];
    last_edges[i][3] = proj_y[idx2];
  }

  
  PD_SetColor(LCD_COLOR_YELLOW);
  for (int i = 0; i < 8; i++) {
    PD_DrawCircle(proj_x[i], proj_y[i], 3);
    last_proj_x[i] = proj_x[i];
    last_proj_y[i] = proj_y[i];
  }

  first_frame = 0;
}