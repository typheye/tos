#include "include/display_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "components/include/alert.hpp"
#include "include/libpd.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

static bool  disp_auto    = false;
static int   disp_bright  = 10;      // 1-10
static int   disp_dir     = 0;       // 0,1 → 0,90
static bool  disp_edit    = false;
static int   edit_field   = 0;
static bool  disp_inited  = false;   // only apply defaults first time       // 1=auto, 2=bright, 3=direction

static const char *dir_names[] = {"0", "90"};

static void apply(void) {
  boardLCD.setBrightness((uint16_t)disp_bright * 100);
  boardLCD.setRotation((uint8_t)disp_dir);
}

// ============ Draw ============

static void draw_frame_title(const char *title) {
  PD_Init(); PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 30000) {
    last_tm = HAL_GetTick();
    Time_t t; Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[6]; sprintf(ts, "%02d:%02d", t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label, const char *value, bool editing) {
  bool s = (idx == sel);
  uint32_t card_c = s ? TOS_ACCENT : TOS_CARD_BG;
  if (editing && s && (HAL_GetTick() / 300) % 2) card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

static int item_count(void) {
  return 5; // Return + Auto + Brightness + Direction + Resolution
}

static void draw_disp(int sel) {
  draw_frame_title("DISP");
  PD_SetFont(FONT_ASCII_16);

  int n = item_count();
  int vis = n < 7 ? n : 7;
  int start = sel - vis / 2;
  if (start < 0) start = 0;
  if (start + vis > n) start = n - vis;

  for (int i = 0; i < vis; i++) {
    int idx = start + i; if (idx >= n) break;
    int cy = 33 + i * 25;

    switch (idx) {
    case 0:
      draw_card(idx, sel, cy, "00 Return");
      break;
    case 1: {
      char buf[32]; snprintf(buf, sizeof(buf), "01 Auto");
      draw_card_r(idx, sel, cy, buf, disp_auto ? "ON" : "OFF", disp_edit && edit_field == 1);
      break;
    }
    case 2: {
      char buf[32]; snprintf(buf, sizeof(buf), "   Brightness");
      char val[8];
      if (disp_auto) val[0] = '\0';
      else snprintf(val, sizeof(val), "%d%%", disp_bright * 10);
      draw_card_r(idx, sel, cy, buf, val, disp_edit && edit_field == 2);
      break;
    }
    case 3: {
      char buf[32]; snprintf(buf, sizeof(buf), "02 Direction");
      draw_card_r(idx, sel, cy, buf, dir_names[disp_dir], disp_edit && edit_field == 3);
      break;
    }
    case 4:
      draw_card(idx, sel, cy, "03 Resolution  240x240");
      break;
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

// ============ Main ============

void display_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  if (!disp_inited) {
    disp_auto = false; disp_bright = 10; disp_dir = 0;
    apply();
    disp_inited = true;
  }
  disp_edit = false; edit_field = 0;
  apply();

  int sel = 0; uint8_t le = 0; uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (disp_edit) {
        if (edit_field == 1) disp_auto = !disp_auto;
        else if (edit_field == 2) { disp_bright++; if (disp_bright > 10) disp_bright = 10; }
        else if (edit_field == 3) { disp_dir = (disp_dir + 1) % 2; apply(); }
      } else {
        sel = (sel + 1) % item_count();
      }
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (disp_edit) {
        if (edit_field == 1) disp_auto = !disp_auto;
        else if (edit_field == 2) { disp_bright--; if (disp_bright < 1) disp_bright = 1; }
        else if (edit_field == 3) { disp_dir = (disp_dir - 1 + 2) % 2; apply(); }
      } else {
        sel = (sel - 1 + item_count()) % item_count();
      }
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (disp_edit) {
        disp_edit = false; edit_field = 0;
        if (!disp_auto) apply();
      } else {
        if (sel == 0) return;
        if (sel == 1) { disp_edit = true; edit_field = 1; }
        if (sel == 2) {
          if (disp_auto) alert_show("ALERT", "Please disable Auto first");
          else { disp_edit = true; edit_field = 2; }
        }
        if (sel == 3) { disp_edit = true; edit_field = 3; }
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick(); draw_disp(sel); }
    HAL_Delay(20);
  }
}
