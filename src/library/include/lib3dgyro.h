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