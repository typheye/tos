/**
 ******************************************************************************
 * @file    pages.cpp
 * @author  Typheye
 * @brief   Pages implementation.
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

#include "include/pages.hpp"
#include "library/include/libdly.h"


extern KeyManager keyManager;
extern LCD boardLCD;
extern THID boardHID;
extern TRTC boardTRTC;
extern JY901S boardJY901S;

static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t;
    Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8];
    time_fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool selected = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, selected ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(selected ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_message(const char *title, const char *line1,
                         const char *line2 = nullptr,
                         const char *line3 = nullptr) {
  LCD_FLUSH({
    draw_frame_title(title);
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    if (line1)
      PD_DrawString(22, 38, line1);
    if (line2)
      PD_DrawString(22, 63, line2);
    if (line3)
      PD_DrawString(22, 88, line3);
  });
}

static void show_message(const char *title, const char *line1,
                         const char *line2 = nullptr,
                         const char *line3 = nullptr, uint32_t hold_ms = 1000) {
  draw_message(title, line1, line2, line3);
  JPDelay(hold_ms);
}

static int page_menu(const char *title, const char **items, int count,
                     int start_sel = 0) {
  int sel = start_sel;
  if (sel >= count)
    sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % count;
      JPDelay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + count) % count;
      JPDelay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return sel;
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title(title);
        int visible = count < 7 ? count : 7;
        int start = sel - visible / 2;
        if (start < 0)
          start = 0;
        if (start + visible > count)
          start = count - visible;
        if (start < 0)
          start = 0;
        for (int i = 0; i < visible; ++i) {
          int idx = start + i;
          draw_card(idx, sel, 33 + i * 25, items[idx]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    JPDelay(1);
  }
}

void hid_tools_status_page(void) {
  char l1[32];
  char l2[32];
  char l3[32];
  snprintf(l1, sizeof(l1), "Configured: %s",
           boardHID.isConfigured() ? "YES" : "NO");
  snprintf(l2, sizeof(l2), "TX: %s", boardHID.isTxIdle() ? "IDLE" : "BUSY");
  snprintf(l3, sizeof(l3), "Vendor report: 0x10");
  show_message("HID", l1, l2, l3, 1600);
}

void hid_tools_vendor_page(void) {
  static const char *items[] = {"00 Return", "01 Send Hello", "02 Read PC OUT",
                                "03 Alert Last RX"};
  char last_rx[64] = "";

  int sel = 0;
  while (1) {
    sel = page_menu("HID", items, 4, sel);
    if (sel == 0)
      return;
    if (sel == 1) {
      char msg[48];
      snprintf(msg, sizeof(msg), "TOS HID HELLO %lu",
               (unsigned long)HAL_GetTick());
      THID::Status st = boardHID.sendVendorText(msg);
      show_message("HID", "Vendor IN sent", boardHID.statusText(st), msg, 1200);
    } else if (sel == 2 || sel == 3) {
      char text[64];
      uint16_t len = 0;
      if (boardHID.receiveVendorText(text, sizeof(text), &len)) {
        strncpy(last_rx, text, sizeof(last_rx) - 1);
        last_rx[sizeof(last_rx) - 1] = '\0';
        char l1[32];
        snprintf(l1, sizeof(l1), "RX %u bytes", (unsigned)len);
        if (sel == 3)
          alert_show("HID", last_rx);
        else
          show_message("HID", l1, last_rx, nullptr, 1500);
      } else {
        show_message("HID", "No PC OUT packet", "Use Python tool send", nullptr,
                     1200);
      }
    }
  }
}

static void send_hotkey(const char *name, uint8_t mod, uint8_t key) {
  char line[32];
  snprintf(line, sizeof(line), "Sending %s", name);
  show_message("HID", line, nullptr, nullptr, 250);
  THID::Status st = boardHID.tapKey(mod, key, 40);
  show_message("HID", name, boardHID.statusText(st), nullptr, 800);
}

void hid_tools_quickkeys_page(void) {
  static const char *items[] = {"00 Return",   "01 Win + L",   "02 Win + D",
                                "03 Win + R",  "04 Alt + Tab", "05 Ctrl + C",
                                "06 Ctrl + V", "07 Type TOS"};

  int sel = 0;
  while (1) {
    sel = page_menu("HID", items, 8, sel);
    if (sel == 0)
      return;
    if (!boardHID.isConfigured()) {
      show_message("HID", "USB not configured", "Reconnect USB first",
                   nullptr, 1000);
      continue;
    }
    switch (sel) {
    case 1:
      send_hotkey("Win+L", THID::MOD_LGUI, HID_KEY_L);
      break;
    case 2:
      send_hotkey("Win+D", THID::MOD_LGUI, HID_KEY_D);
      break;
    case 3:
      send_hotkey("Win+R", THID::MOD_LGUI, HID_KEY_R);
      break;
    case 4:
      send_hotkey("Alt+Tab", THID::MOD_LALT, HID_KEY_TAB);
      break;
    case 5:
      send_hotkey("Ctrl+C", THID::MOD_LCTRL, HID_KEY_C);
      break;
    case 6:
      send_hotkey("Ctrl+V", THID::MOD_LCTRL, HID_KEY_V);
      break;
    case 7: {
      THID::Status st = boardHID.typeAscii("TOS HID OK", 20);
      show_message("HID", "Typed TOS HID OK", boardHID.statusText(st),
                   nullptr, 900);
      break;
    }
    default:
      break;
    }
  }
}

static void mouse_wiggle(void) {
  show_message("HID", "Wiggle pointer", nullptr, nullptr, 350);
  THID::Status st = THID::OK;
  for (int i = 0; i < 8; i++) {
    st = boardHID.moveMouse(12, 0);
    JPDelay(18);
  }
  for (int i = 0; i < 8; i++) {
    st = boardHID.moveMouse(-12, 0);
    JPDelay(18);
  }
  show_message("HID", "Mouse test done", boardHID.statusText(st), nullptr,
               800);
}

void hid_tools_mouse_page(void) {
  static const char *items[] = {"00 Return",     "01 Wiggle",
                                "02 Left Click", "03 Right Click",
                                "04 Scroll Up",  "05 Scroll Down"};
  int sel = 0;
  while (1) {
    sel = page_menu("HID", items, 6, sel);
    if (sel == 0)
      return;
    if (!boardHID.isConfigured()) {
      show_message("HID", "USB not configured", "Reconnect USB first",
                   nullptr, 1000);
      continue;
    }
    THID::Status st = THID::OK;
    if (sel == 1)
      mouse_wiggle();
    else if (sel == 2)
      st = boardHID.clickMouse(THID::MOUSE_LEFT, 35);
    else if (sel == 3)
      st = boardHID.clickMouse(THID::MOUSE_RIGHT, 35);
    else if (sel == 4)
      st = boardHID.scrollMouse(4);
    else if (sel == 5)
      st = boardHID.scrollMouse(-4);
    if (sel != 1)
      show_message("HID", "Action sent", boardHID.statusText(st), nullptr,
                   700);
  }
}

#define GYRO_MOUSE_MAX_DELTA 120
#define GYRO_MOUSE_SPEED_GAIN 2.00f

static int8_t clamp_mouse_delta(int v) {
  if (v > GYRO_MOUSE_MAX_DELTA)
    return GYRO_MOUSE_MAX_DELTA;
  if (v < -GYRO_MOUSE_MAX_DELTA)
    return -GYRO_MOUSE_MAX_DELTA;
  return (int8_t)v;
}

static int8_t gyro_rate_to_delta(float rate_dps, float *fraction_accum) {
  const float dead_zone_dps = 4.0f;
  float a = fabsf(rate_dps);
  if (a < dead_zone_dps) {
    *fraction_accum *= 0.60f;
    return 0;
  }
  float delta = (a - dead_zone_dps) * GYRO_MOUSE_SPEED_GAIN;
  if (rate_dps < 0.0f)
    delta = -delta;
  *fraction_accum += delta;
  int whole = (int)(*fraction_accum);
  if (whole > GYRO_MOUSE_MAX_DELTA)
    whole = GYRO_MOUSE_MAX_DELTA;
  else if (whole < -GYRO_MOUSE_MAX_DELTA)
    whole = -GYRO_MOUSE_MAX_DELTA;
  *fraction_accum -= (float)whole;
  return clamp_mouse_delta(whole);
}

static void draw_gyro_mouse_status(float gx, float gz, int8_t dx, int8_t dy,
                                   uint8_t buttons) {
  LCD_FLUSH({
    char line[48];
    draw_frame_title("HID");
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(18, 34, "Gyro Mouse");
    PD_SetColor(TOS_TEXT_SEC);
    snprintf(line, sizeof(line), "X=Gz:%ld  Y=-Gx:%ld", (long)gz, (long)(-gx));
    PD_DrawString(18, 58, line);
    snprintf(line, sizeof(line), "dX:%d dY:%d", (int)dx, (int)dy);
    PD_DrawString(18, 80, line);
    snprintf(line, sizeof(line), "A8:L=%s D0:R=%s",
             (buttons & THID::MOUSE_LEFT) ? "ON" : "--",
             (buttons & THID::MOUSE_RIGHT) ? "ON" : "--");
    PD_DrawString(18, 102, line);
    PD_DrawFooterCenter("ENTER", "LEFT", "RIGHT");
  });
}

void hid_tools_gyro_mouse_page(void) {
  if (!boardJY901S.isInitialized())
    boardJY901S.init();
  if (!boardHID.isConfigured()) {
    show_message("HID", "USB not configured", "Reconnect USB first", nullptr,
                 1200);
    return;
  }

  show_message("HID", "Move by JY901S gyro", "A8=L  D0=R", "ENTER exits", 1000);

  float smooth_x = 0.0f, smooth_z = 0.0f;
  float frac_x = 0.0f, frac_y = 0.0f;
  int8_t last_dx = 0, last_dy = 0;
  uint8_t last_buttons = 0xFFU;
  uint32_t last_report = 0, last_ui = 0;

  while (1) {
    keyManager.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      if (boardHID.isConfigured())
        (void)boardHID.releaseMouse();
      show_message("HID", "Stopped", "Mouse released", nullptr, 500);
      return;
    }

    uint32_t now = HAL_GetTick();
    if (now - last_report >= 16U) {
      last_report = now;
      JY901S_Data_t data = boardJY901S.readData();
      smooth_x = smooth_x * 0.72f + data.gyro_x * 0.28f;
      smooth_z = smooth_z * 0.72f + data.gyro_z * 0.28f;
      int8_t dx = gyro_rate_to_delta(-smooth_z, &frac_x);
      int8_t dy = gyro_rate_to_delta(-smooth_x, &frac_y);
      uint8_t buttons = 0;
      if (keyManager.collision_A8.isPressed())
        buttons |= THID::MOUSE_LEFT;
      if (keyManager.collision_D0.isPressed())
        buttons |= THID::MOUSE_RIGHT;
      if (boardHID.isConfigured() && boardHID.isTxIdle() &&
          (dx != 0 || dy != 0 || buttons != last_buttons)) {
        (void)boardHID.sendMouse(buttons, dx, dy, 0);
        last_buttons = buttons;
      }
      last_dx = dx;
      last_dy = dy;
      if (now - last_ui >= 500U) {
        last_ui = now;
        draw_gyro_mouse_status(smooth_x, smooth_z, last_dx, last_dy, buttons);
      }
    }
    JPDelay(1);
  }
}
