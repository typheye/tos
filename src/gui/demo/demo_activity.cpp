#include "include/demo_activity.hpp"
#include "core/include/systime.h"
#include "gui/include/settings.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/3dox_activity.hpp"
#include "include/bmp_activity.hpp"
#include "include/i2c_activity.hpp"
#include "include/jyro_activity.hpp"
#include "include/key_activity.hpp"
#include "include/libpd.h"
#include "include/pot_activity.hpp"
#include "include/sd_activity.hpp"
#include "include/sn74hc00n_activity.hpp"
#include "include/tcs3472_activity.hpp"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

#define DEMO_ITEMS 10
static const char *demo_m[DEMO_ITEMS] = {
    "00 Return",         "01 Key Test",      "02 SD Card Test",
    "03 I2C Scan",       "04 JY901S Sensor", "05 BMP180 Sensor",
    "07 3D Path Tracer", "09 TCS3472 Test",  "10 SN74HC00N Test",
    "11 Pot Test",
};
static void (*demo_f[DEMO_ITEMS])(void) = {
    NULL,
    key_test_activity,
    sd_card_activity,
    i2c_scan_activity,
    jyro_activity,
    bmp180_activity,
    render_3dox_activity_with_exit,
    tcs3472_activity,
    hc00n_activity,
    pot_activity,
};

// ============ Original TOS-style menu ============

static void draw_menu(const char *title, const char **items, int count,
                      int sel) {
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    // Refresh header time
    extern TRTC boardTRTC;
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
    if (start < 0)
      start = 0;
    if (start + visible > count)
      start = count - visible;

    for (int i = 0; i < visible; i++) {
      int idx = start + i;
      if (idx >= count)
        break;
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
    HAL_Delay(1);
  }
}

void demo_list_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  static int sel = 0;
  while (1) {
    sel = menu_loop("TOS", demo_m, DEMO_ITEMS, sel);
    if (sel == 0)
      return;
    if (demo_f[sel]) {
      boardLCD.fillScreen(LCD_COLOR_BLACK);
      demo_f[sel]();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}
