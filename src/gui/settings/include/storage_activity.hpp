/**
 ******************************************************************************
 * @file    storage_activity.hpp
 * @author  Typheye
 * @brief   Storage Activity interface.
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

#ifndef STORAGE_ACTIVITY_HPP
#define STORAGE_ACTIVITY_HPP
#include "core/sys/include/systime.h"
#include "core/sys/include/syshandle.h"
#include "ff.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "hardware/include/tsdio.hpp"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"
#include <cstdio>

void storage_activity_run(void);
#endif
