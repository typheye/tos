/**
 ******************************************************************************
 * @file    status_icons.cpp
 * @author  Typheye
 * @brief   Status Icons implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/status_icons.h"


/* Include the auto-generated icon data */

/* Pixel callback for icon_draw_bitmap_msb */
static void put_pixel(int x, int y, void *user) {
  uint32_t color = *(uint32_t *)user;
  PD_SetColor(color);
  PD_DrawPixel((int16_t)x, (int16_t)y);
}

/* ==================================================================
 *  WLAN icon — wifi_on / wifi_off bitmaps (25x18 px)
 * ================================================================== */
void draw_icon_wifi(int x, int y, bool on) {
  uint32_t color = on ? TOS_ACCENT : TOS_CARD_BG;
  const IconBitmap *bm = on ? &ICON_WIFI_ON : &ICON_WIFI_OFF;
  icon_draw_bitmap_msb(bm, x, y, put_pixel, &color);
}

/* ==================================================================
 *  Signal icon — signal_on / signal_off bitmaps
 * ================================================================== */
void draw_icon_signal(int x, int y, int signal) {
  uint32_t color = signal > 0 ? TOS_ACCENT : TOS_CARD_BG;
  const IconBitmap *bm = signal > 0 ? &ICON_SIGNAL_ON : &ICON_SIGNAL_OFF;
  icon_draw_bitmap_msb(bm, x, y, put_pixel, &color);
}

/* ==================================================================
 *  All-in-one
 * ================================================================== */
/* ==================================================================
 *  Title-bar ico icon
 * ================================================================== */
void draw_icon_ico(void) {
  uint32_t color = TOS_ACCENT;
  icon_draw_bitmap_msb(&ICON_ICO, 0, 0, put_pixel, &color);
}

/* ==================================================================
 *  All-in-one
 * ================================================================== */
void status_icons_draw(bool wlan_on, bool wlan_connected, bool hotspot_on) {
  /* Signal — x=104, y=1 */
  draw_icon_signal(104, 1, wlan_connected ? 100 : 0);

  /* WLAN — x=124 */
  draw_icon_wifi(124, 0, wlan_on);

  /* Hotspot — x=158 */
  if (hotspot_on) {
    uint32_t color = TOS_ACCENT;
    icon_draw_bitmap_msb(&ICON_HOTSPOT_ON, 158, 1, put_pixel, &color);
  }
}
