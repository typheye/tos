/**
 ******************************************************************************
 * @file    keyboard.hpp
 * @author  Typheye
 * @brief   Keyboard interface.
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

#ifndef KEYBOARD_HPP
#define KEYBOARD_HPP
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include "include/pot.hpp"
#include <cstdio>
#include <cstring>
#include "core/sys/include/syslog.h"

// Opens an ASCII keyboard overlay for password entry.
// title: shown at top (e.g. "WiFi Password")
// max_len: max password length (1~24)
// out: buffer to receive the password (caller-allocated, must be >= max_len+1)
// Returns: true if user confirmed (pressed Connect), false if cancelled.
bool keyboard_open(const char *title, char *out, int max_len);

#endif
