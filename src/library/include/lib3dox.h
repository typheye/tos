#ifndef __LIB3D_H
#define __LIB3D_H

#include <stdint.h>

#define RENDER_WIDTH 240
#define RENDER_HEIGHT 240

typedef void (*pixel_callback_t)(int x, int y, uint32_t color);

extern volatile int render_progress;

#ifdef __cplusplus
extern "C" {
#endif

void render_init(void);
int render_step(pixel_callback_t pixel_cb);
int get_render_progress(void);

#ifdef __cplusplus
}
#endif

#endif