/**
 ******************************************************************************
 * @file    lib3dgyro.h
 * @author  Typheye
 * @brief   Lib3Dgyro interface.
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

#ifndef __LIB3DGYRO_H
#define __LIB3DGYRO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gyro_cube_init(int16_t center_x, int16_t center_y, int16_t size);
void gyro_cube_draw(float roll, float pitch, float yaw);

#ifdef __cplusplus
}
#endif

#endif