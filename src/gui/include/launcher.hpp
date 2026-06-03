/**
 ******************************************************************************
 * @file    launcher.hpp
 * @author  Typheye
 * @brief   Launcher interface.
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

#ifndef LAUNCHER_HPP
#define LAUNCHER_HPP
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "core/manager/include/emotion_manager.h"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "library/include/libehw.h"
#include "library/include/libemo.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "core/sys/include/syslog.h"

// Run the pet launcher loop. Returns when user long-presses Enter.
void pet_launcher_run(void);

#endif
