/**
 ******************************************************************************
 * @file    syslogo.h
 * @brief   Legacy syslogo declaration shim. Logo data lives in SAH/include/sah_logo.h.
 ******************************************************************************
 */

#ifndef __LOGO_DATA_H
#define __LOGO_DATA_H

#include <stdint.h>

#define LOGO_WIDTH  240
#define LOGO_HEIGHT 240

extern const uint16_t logo_data[];

#endif /* __LOGO_DATA_H */