/**
 * @file    status_icons.h
 * @brief   Status-bar icon drawing
 */

#ifndef STATUS_ICONS_H
#define STATUS_ICONS_H

#include <stdint.h>
#include <stdbool.h>

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
