#include "include/app.h"
#include "include/hid_tools_pages.hpp"
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern TRTC boardTRTC;

#define HID_TOOLS_MENU_ITEMS 6
static const char *hid_tools_menu[HID_TOOLS_MENU_ITEMS] = {
    "00 Return",
    "01 USB Status",
    "02 Vendor IO",
    "03 Quick Keys",
    "04 Mouse Test",
    "05 Gyro Mouse",
};

typedef void (*HidPageFn)(void);
static HidPageFn hid_tools_pages[HID_TOOLS_MENU_ITEMS] = {
    nullptr,
    hid_tools_status_page,
    hid_tools_vendor_page,
    hid_tools_quickkeys_page,
    hid_tools_mouse_page,
    hid_tools_gyro_mouse_page,
};

static void draw_menu(const char *title, const char **items, int count, int sel) {
  LCD_FLUSH({
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

    int visible = count < 7 ? count : 7;
    int start = sel - visible / 2;
    if (start < 0) start = 0;
    if (start + visible > count) start = count - visible;
    if (start < 0) start = 0;

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
      PD_DrawString(26, cy + 2, items[idx]);
    }
    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

static int menu_loop(const char *title, const char **items, int count,
                     int start_sel) {
  int sel = start_sel;
  if (sel >= count) sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    TosApi_Tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % count;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + count) % count;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return sel;
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_menu(title, items, count, sel);
    }
    TosApi_Tick();
    HAL_Delay(1);
  }
}

void hid_tools_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  static int sel = 0;

  while (1) {
    sel = menu_loop("HID", hid_tools_menu, HID_TOOLS_MENU_ITEMS, sel);
    if (sel == 0) return;
    if (hid_tools_pages[sel] != nullptr) {
      boardLCD.fillScreen(LCD_COLOR_BLACK);
      hid_tools_pages[sel]();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}
