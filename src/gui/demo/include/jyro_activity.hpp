/**
 ******************************************************************************
 * @file    jyro_activity.hpp
 * @author  Typheye
 * @brief   Jyro Activity interface.
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

#ifndef __JYRO_ACTIVITY_HPP
#define __JYRO_ACTIVITY_HPP
#include "core/sys/include/systime.h"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include "core/sys/include/syslog.h"
#include <cstdint>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif


void jyro_activity(void);


void jyro_cube_activity(void);


void jyro_text_activity(void);


void jyro_chart_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __JYRO_ACTIVITY_HPP */