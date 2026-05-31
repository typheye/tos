#include "include/hid_activity.hpp"
#include "core/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/thid.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;
extern THID boardHID;
extern TRTC boardTRTC;

#define HID_MENU_ITEMS 7
static const char *hid_menu[HID_MENU_ITEMS] = {
    "00 Return",
    "01 USB Status",
    "02 Vendor Hello",
    "03 Read PC OUT",
    "04 Win + L",
    "05 Type TOS",
    "06 Mouse Wiggle",
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
  LCD_Flush();
}

static void show_message(const char *title, const char *line1,
                         const char *line2 = nullptr,
                         const char *line3 = nullptr,
                         uint32_t hold_ms = 1200) {
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
  LCD_Flush();
  HAL_Delay(hold_ms);
}

static void show_status(void) {
  char l1[32];
  char l2[32];
  snprintf(l1, sizeof(l1), "Configured: %s", boardHID.isConfigured() ? "YES" : "NO");
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
    show_message("HID", "No PC OUT packet", "Use Python tool send", nullptr, 1200);
  }
}

static void send_win_l(void) {
  show_message("HID", "Sending Win+L", "PC may lock screen", nullptr, 500);
  THID::Status st = boardHID.sendWinLock();
  show_message("HID", "Win+L sent", boardHID.statusText(st), nullptr, 900);
}

static void type_tos(void) {
  show_message("HID", "Typing test text", "Focus a text box first", nullptr, 700);
  THID::Status st = boardHID.typeAscii("TOS HID OK", 20);
  show_message("HID", "Keyboard type done", boardHID.statusText(st), nullptr, 900);
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

void hid_test_activity_run(void) {
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
      }
      last_ui = 0;
    }
    last_enter = enter;

    if (HAL_GetTick() - last_ui > 100) {
      last_ui = HAL_GetTick();
      draw_menu(sel);
    }
    HAL_Delay(20);
  }
}
