/**
 ******************************************************************************
 * @file    app.h
 * @author  Typheye
 * @brief   App interface.
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

#ifndef HID_TOOLS_APP_H
#define HID_TOOLS_APP_H
#ifdef __cplusplus
#include "gui/miniapp/hid_tools/include/hid_tools_pages.hpp"
#endif
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#ifdef __cplusplus
#include "hardware/include/key.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/lcd.hpp"
#endif
#ifdef __cplusplus
#include "hardware/include/trtc.hpp"
#endif
#include "include/libpd.h"

#ifdef __cplusplus
extern "C" {
#endif

void hid_tools_run(void);

#ifdef __cplusplus
}
#endif

#endif
