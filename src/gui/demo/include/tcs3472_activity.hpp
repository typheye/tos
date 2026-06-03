/**
 ******************************************************************************
 * @file    tcs3472_activity.hpp
 * @author  Typheye
 * @brief   Tcs3472 Activity interface.
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

#ifndef __TCS3472_ACTIVITY_HPP
#define __TCS3472_ACTIVITY_HPP
#include "components/include/alert.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/tcs3472.hpp"
#include "core/sys/include/syslog.h"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


void tcs3472_activity(void);


void tcs3472_color_demo(void);


void tcs3472_cct_demo(void);


void tcs3472_read_activity(void);


void tcs3472_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCS3472_ACTIVITY_HPP */