/**
 ******************************************************************************
 * @file    status_icons.h
 * @author  Typheye
 * @brief   Status Icons interface.
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

#ifndef STATUS_ICONS_H
#define STATUS_ICONS_H

#include <stdint.h>
#include <stdbool.h>
#include "include/libpd.h"
#include "gui/components/include/icons.h"

#ifdef __cplusplus
extern "C" {
#endif

void draw_icon_signal(int x, int y, int signal);
void draw_icon_wifi(int x, int y, bool on);
void draw_icon_ico(void);
void status_icons_draw(bool wlan_on, bool wlan_connected, bool hotspot_on);

#ifdef __cplusplus
}
#endif

#endif
