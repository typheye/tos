/**
 ******************************************************************************
 * @file    libui.cpp
 * @brief   Shared UI drawing helpers.
 ******************************************************************************
 */

#include "include/libui.h"

#include "core/sys/include/systime.h"
#include "core/sdk/include/tos_api.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libdly.h"
#include "include/libpd.h"
#include "gui/components/include/status_icons.h"

extern TRTC boardTRTC;
extern KeyManager keyManager;

extern "C" bool esp_wlan_is_on(void);
extern "C" bool esp_wlan_is_connected(void);
extern "C" bool hotspot_is_active(void);

void UI_DrawFrameTitle(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);

  static uint32_t last_tm = 0;
  if ((uint32_t)(HAL_GetTick() - last_tm) > 1000U) {
    last_tm = HAL_GetTick();
    Time_t t;
    Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8];
    SysTime_Fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }

  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

void UI_DrawFrameStatusIcons(void) {
  draw_icon_ico();
  status_icons_draw(esp_wlan_is_on(), esp_wlan_is_connected(),
                    hotspot_is_active());
}

void UI_DrawMenuCardEx(int idx, int sel, int cy, const char *text, bool grey) {
  bool selected = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5,
                    selected ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(grey ? TOS_GREY : (selected ? TOS_TEXT : TOS_TEXT_SEC));
  PD_DrawString(26, cy + 2, text);
}

void UI_DrawMenuCard(int idx, int sel, int cy, const char *text) {
  UI_DrawMenuCardEx(idx, sel, cy, text, false);
}

void UI_DrawMenuValue(int idx, int sel, int cy, const char *label,
                      const char *value, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  if (editing && selected && ((HAL_GetTick() / 300U) & 1U)) {
    card_c = TOS_CARD_BG;
  }

  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(selected ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);

  if (value) {
    uint16_t vw = PD_GetStringWidth(value);
    PD_DrawString(220 - vw, cy + 2, value);
  }
}

void UI_DrawMenuValueEx(int idx, int sel, int cy, const char *label,
                        const char *value, bool editing, bool grey) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  if (editing && selected && ((HAL_GetTick() / 300U) & 1U)) {
    card_c = TOS_CARD_BG;
  }

  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(grey ? TOS_GREY : (selected ? TOS_TEXT : TOS_TEXT_SEC));
  PD_DrawString(26, cy + 2, label);

  if (value) {
    uint16_t vw = PD_GetStringWidth(value);
    PD_DrawString(220 - vw, cy + 2, value);
  }
}

void UI_DrawMenuBool(int idx, int sel, int cy, const char *label, bool on,
                     bool editing) {
  UI_DrawMenuValue(idx, sel, cy, label, on ? "ON" : "OFF", editing);
}

void UI_DrawStdFooter(void) {
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
}

int UI_MenuVisibleStart(int count, int sel, int visible) {
  if (visible > count) {
    visible = count;
  }
  int start = sel - visible / 2;
  if (start < 0) {
    start = 0;
  }
  if (start + visible > count) {
    start = count - visible;
  }
  return start < 0 ? 0 : start;
}

void UI_DrawSimpleMenu(const char *title, const char **items, int count,
                       int sel) {
  LCD_FLUSH({
    UI_DrawFrameTitle(title);
    PD_SetFont(FONT_ASCII_16);

    int visible = count < 7 ? count : 7;
    int start = UI_MenuVisibleStart(count, sel, visible);
    for (int i = 0; i < visible; i++) {
      int idx = start + i;
      if (idx >= count) {
        break;
      }
      UI_DrawMenuCard(idx, sel, 33 + i * 25, items[idx]);
    }
    UI_DrawStdFooter();
  });
}

int UI_MenuLoop(const char *title, const char **items, int count,
                int start_sel) {
  int sel = start_sel;
  if (sel >= count) {
    sel = 0;
  }
  uint8_t last_enter = 0;
  uint32_t last_update = 0;

  while (1) {
    TosApi_Tick();
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % count;
      JPDelay(80);
    }
    if (keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + count) % count;
      JPDelay(80);
    }

    uint8_t enter = (keyManager._btnEnter.getState() == KEY_PRESSED);
    if (enter && !last_enter) {
      return sel;
    }
    last_enter = enter;

    if ((uint32_t)(HAL_GetTick() - last_update) > 16U) {
      last_update = HAL_GetTick();
      UI_DrawSimpleMenu(title, items, count, sel);
    }
    TosApi_Tick();
    JPDelay(1);
  }
}
