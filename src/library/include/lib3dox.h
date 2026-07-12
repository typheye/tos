/**
 ******************************************************************************
 * @file    lib3dox.h
 * @author  Typheye
 * @brief   Lib3Dox interface.
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

#ifndef LIB3D_H
#define LIB3D_H

#include <stdint.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RENDER_WIDTH 160
#define RENDER_HEIGHT 80

typedef void (*pixel_callback_t)(int x, int y, uint32_t color);

extern volatile int render_progress;

#ifdef __cplusplus
extern "C" {
#endif

void render_init(void);
void render_deinit(void);
int render_is_ready(void);
int render_step(pixel_callback_t pixel_cb);
int get_render_progress(void);
uint16_t render_get_pixel565(int x, int y);
int render_get_frame_count(void);

#ifdef __cplusplus
}
#endif

#endif /* LIB3DOX_H */
