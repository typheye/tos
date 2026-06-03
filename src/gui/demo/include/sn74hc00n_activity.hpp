/**
 ******************************************************************************
 * @file    sn74hc00n_activity.hpp
 * @author  Typheye
 * @brief   Sn74Hc00N Activity interface.
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

#ifndef __SN74HC00N_ACTIVITY_HPP
#define __SN74HC00N_ACTIVITY_HPP
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/sn74hc00n.hpp"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


void hc00n_activity(void);


void hc00n_monitor_activity(void);


void hc00n_truth_table_activity(void);


void hc00n_test_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __SN74HC00N_ACTIVITY_HPP */