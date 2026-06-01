#include "include/app.h"
#include "core/include/systime.h"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/thid.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>
#include <cstring>
#include <math.h>

extern KeyManager keyManager;
extern LCD boardLCD;
extern THID boardHID;
extern TRTC boardTRTC;
extern JY901S boardJY901S;

#define HID_MENU_ITEMS 8
static const char *hid_menu[HID_MENU_ITEMS] = {
    "00 Return",  "01 USB Status", "02 Vendor Hello", "03 Read PC OUT",
    "04 Win + L", "05 Type TOS",   "06 Mouse Wiggle", "07 Gyro Mouse",
};

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

static void draw_menu(int sel) {
  LCD_FLUSH({
    draw_frame_title("HID");

    int visible = HID_MENU_ITEMS < 7 ? HID_MENU_ITEMS : 7;
    int start = sel - visible / 2;
    if (start < 0) {
      start = 0;
    }
    if (start + visible > HID_MENU_ITEMS) {
      start = HID_MENU_ITEMS - visible;
    }

    for (int i = 0; i < visible; i++) {
      int idx = start + i;
      int cy = 33 + i * 25;
      if (idx == sel) {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
        PD_SetColor(TOS_TEXT);
      } else {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
        PD_SetColor(TOS_TEXT_SEC);
      }
      PD_DrawString(26, cy + 2, hid_menu[idx]);
    }

    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

static void show_message(const char *title, const char *line1,
                         const char *line2 = nullptr,
                         const char *line3 = nullptr, uint32_t hold_ms = 1200) {
  LCD_FLUSH({
    draw_frame_title(title);
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    if (line1 != nullptr) {
      PD_DrawString(22, 38, line1);
    }
    if (line2 != nullptr) {
      PD_DrawString(22, 63, line2);
    }
    if (line3 != nullptr) {
      PD_DrawString(22, 88, line3);
    }
    PD_DrawFooterCenter("WAIT", NULL, "");
  });
  HAL_Delay(hold_ms);
}

static void show_status(void) {
  char l1[32];
  char l2[32];
  snprintf(l1, sizeof(l1), "Configured: %s",
           boardHID.isConfigured() ? "YES" : "NO");
  snprintf(l2, sizeof(l2), "TX: %s", boardHID.isTxIdle() ? "IDLE" : "BUSY");
  show_message("HID", l1, l2, "Report: KBD/MOUSE/VENDOR", 1500);
}

static void send_vendor_hello(void) {
  char msg[48];
  snprintf(msg, sizeof(msg), "TOS HID HELLO %lu", (unsigned long)HAL_GetTick());
  THID::Status st = boardHID.sendVendorText(msg);
  show_message("HID", "Vendor IN sent", boardHID.statusText(st), msg, 1200);
}

static void read_pc_out(void) {
  char text[64];
  uint16_t len = 0;
  if (boardHID.receiveVendorText(text, sizeof(text), &len)) {
    char l1[32];
    snprintf(l1, sizeof(l1), "RX %u bytes", (unsigned)len);
    show_message("HID", l1, text, nullptr, 1800);
  } else {
    show_message("HID", "No PC OUT packet", "Use Python tool send", nullptr,
                 1200);
  }
}

static void send_win_l(void) {
  show_message("HID", "Sending Win+L", "PC may lock screen", nullptr, 500);
  THID::Status st = boardHID.sendWinLock();
  show_message("HID", "Win+L sent", boardHID.statusText(st), nullptr, 900);
}

static void type_tos(void) {
  show_message("HID", "Typing test text", "Focus a text box first", nullptr,
               700);
  THID::Status st = boardHID.typeAscii("TOS HID OK", 20);
  show_message("HID", "Keyboard type done", boardHID.statusText(st), nullptr,
               900);
}

static void mouse_wiggle(void) {
  show_message("HID", "Mouse wiggle", "Moving pointer", nullptr, 500);
  THID::Status st = THID::OK;
  for (int i = 0; i < 8; i++) {
    st = boardHID.moveMouse(12, 0);
    HAL_Delay(18);
  }
  for (int i = 0; i < 8; i++) {
    st = boardHID.moveMouse(-12, 0);
    HAL_Delay(18);
  }
  show_message("HID", "Mouse test done", boardHID.statusText(st), nullptr, 900);
}

#define GYRO_MOUSE_MAX_DELTA 120
#define GYRO_MOUSE_SPEED_GAIN 2.00f

static int8_t clamp_mouse_delta(int v) {
  if (v > GYRO_MOUSE_MAX_DELTA) {
    return GYRO_MOUSE_MAX_DELTA;
  }
  if (v < -GYRO_MOUSE_MAX_DELTA) {
    return -GYRO_MOUSE_MAX_DELTA;
  }
  return (int8_t)v;
}

static int8_t gyro_rate_to_delta(float rate_dps, float *fraction_accum) {
  const float dead_zone_dps = 4.0f;
  const float gain = GYRO_MOUSE_SPEED_GAIN;
  float a = fabsf(rate_dps);

  if (a < dead_zone_dps) {
    *fraction_accum *= 0.60f;
    return 0;
  }

  float delta = (a - dead_zone_dps) * gain;
  if (rate_dps < 0.0f) {
    delta = -delta;
  }

  *fraction_accum += delta;
  int whole = (int)(*fraction_accum);
  if (whole > GYRO_MOUSE_MAX_DELTA) {
    whole = GYRO_MOUSE_MAX_DELTA;
  } else if (whole < -GYRO_MOUSE_MAX_DELTA) {
    whole = -GYRO_MOUSE_MAX_DELTA;
  }
  *fraction_accum -= (float)whole;
  return clamp_mouse_delta(whole);
}

static void draw_gyro_mouse_status(float gx, float gy, float gz, int8_t dx,
                                   int8_t dy, uint8_t buttons) {
  LCD_FLUSH({
    char line[48];
    draw_frame_title("Gyro Mouse");

    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(18, 34, "JY901S -> HID Mouse x10");

    PD_SetColor(TOS_TEXT_SEC);
    snprintf(line, sizeof(line), "X=Gz:%ld  Y=-Gx:%ld", (long)gz, (long)(-gx));
    PD_DrawString(18, 58, line);

    snprintf(line, sizeof(line), "dX:%d dY:%d", (int)dx, (int)dy);
    PD_DrawString(18, 80, line);

    snprintf(line, sizeof(line), "A8:L=%s D0:R=%s",
             (buttons & THID::MOUSE_LEFT) ? "ON" : "--",
             (buttons & THID::MOUSE_RIGHT) ? "ON" : "--");
    PD_DrawString(18, 102, line);

    snprintf(line, sizeof(line), "Gain x10 Max %d", GYRO_MOUSE_MAX_DELTA);
    PD_DrawString(18, 124, line);

    snprintf(line, sizeof(line), "Gyro X/Y/Z %ld/%ld/%ld", (long)gx, (long)gy,
             (long)gz);
    PD_DrawString(18, 146, line);

    PD_DrawFooterCenter("ENTER", "LEFT", "RIGHT");
  });
}

static void gyro_mouse_activity(void) {
  if (!boardJY901S.isInitialized()) {
    boardJY901S.init();
  }

  if (!boardHID.isConfigured()) {
    show_message("Gyro Mouse", "USB not configured", "Reconnect USB first",
                 nullptr, 1200);
    return;
  }

  show_message("Gyro Mouse", "Move by JY901S gyro", "A8=L  D0=R", "ENTER exits",
               1000);

  float smooth_x = 0.0f;
  float smooth_y = 0.0f;
  float smooth_z = 0.0f;
  float frac_x = 0.0f;
  float frac_y = 0.0f;
  int8_t last_dx = 0;
  int8_t last_dy = 0;
  uint8_t last_buttons = 0xFFU;
  uint32_t last_report = 0;
  uint32_t last_ui = 0;

  while (1) {
    keyManager.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      if (boardHID.isConfigured()) {
        (void)boardHID.releaseMouse();
      }
      show_message("Gyro Mouse", "Stopped", "Mouse released", nullptr, 500);
      return;
    }

    uint32_t now = HAL_GetTick();
    if (now - last_report >= 16U) {
      last_report = now;

      JY901S_Data_t data = boardJY901S.readData();

      smooth_x = smooth_x * 0.72f + data.gyro_x * 0.28f;
      smooth_y = smooth_y * 0.72f + data.gyro_y * 0.28f;
      smooth_z = smooth_z * 0.72f + data.gyro_z * 0.28f;

      int8_t dx = gyro_rate_to_delta(smooth_z, &frac_x);
      int8_t dy = gyro_rate_to_delta(-smooth_x, &frac_y);

      uint8_t buttons = 0;
      if (keyManager.collision_A8.isPressed()) {
        buttons |= THID::MOUSE_LEFT;
      }
      if (keyManager.collision_D0.isPressed()) {
        buttons |= THID::MOUSE_RIGHT;
      }

      if (boardHID.isConfigured() && boardHID.isTxIdle() &&
          (dx != 0 || dy != 0 || buttons != last_buttons)) {
        (void)boardHID.sendMouse(buttons, dx, dy, 0);
        last_buttons = buttons;
      }

      last_dx = dx;
      last_dy = dy;

      if (now - last_ui >= 160U) {
        last_ui = now;
        draw_gyro_mouse_status(smooth_x, smooth_y, smooth_z, last_dx, last_dy,
                               buttons);
      }
    }

    HAL_Delay(1);
  }
}

void hid_tools_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  int sel = 0;
  uint8_t last_enter = 0;
  uint32_t last_ui = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % HID_MENU_ITEMS;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + HID_MENU_ITEMS) % HID_MENU_ITEMS;
      HAL_Delay(150);
    }

    uint8_t enter = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (enter && !last_enter) {
      if (sel == 0) {
        return;
      } else if (sel == 1) {
        show_status();
      } else if (sel == 2) {
        send_vendor_hello();
      } else if (sel == 3) {
        read_pc_out();
      } else if (sel == 4) {
        send_win_l();
      } else if (sel == 5) {
        type_tos();
      } else if (sel == 6) {
        mouse_wiggle();
      } else if (sel == 7) {
        gyro_mouse_activity();
      }
      last_ui = 0;
    }
    last_enter = enter;

    if (HAL_GetTick() - last_ui > 100) {
      last_ui = HAL_GetTick();
      draw_menu(sel);
    }
    HAL_Delay(1);
  }
}
