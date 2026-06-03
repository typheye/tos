/**
 ******************************************************************************
 * @file    bmp_activity.hpp
 * @author  Typheye
 * @brief   Bmp Activity interface.
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

#ifndef __BMP_ACTIVITY_HPP
#define __BMP_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


void bmp180_activity(void);


void bmp180_display_activity(void);


void bmp180_chart_activity(void);


void bmp180_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMP_ACTIVITY_HPP */