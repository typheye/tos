#include "include/sn74hc00n_activity.hpp"
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/sn74hc00n.hpp"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;
extern SN74HC00N boardHC00N;

/* ── Standard template functions (exact copy from key_test) ── */
static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
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
}

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

/* ── 01 Monitor sub-page (scrollable menu, key_test pattern) ── */
static void hc00n_monitor_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 5;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 5) % 5;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();
      uint8_t outputs = boardHC00N.readOutputByte();

      LCD_FLUSH({
        draw_frame_title("DEMO");

        PD_SetFont(FONT_ASCII_16);
        int cy = 33;

        /* Item 0: 00 Return */
        draw_card(0, sel, cy, "00 Return");

        /* Items 1-4: CH1 - CH4 */
        for (int ch = 0; ch < 4; ch++) {
          cy += 25;
          uint8_t state = (outputs >> (3 - ch)) & 0x01;

          char label[8];
          snprintf(label, sizeof(label), " - CH%d", ch + 1);

          const char *val = state ? "LOW" : "HIGH";

          draw_card_r(ch + 1, sel, cy, label, val);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

/* ── 02 Truth Table sub-page (scrollable menu, key_test pattern) ── */
static void hc00n_truth_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  /*
   * NAND truth table (Y = NOT (A AND B)):
   *   A=0 B=0 -> Y=1  (TOS_TEXT)
   *   A=1 B=0 -> Y=1  (TOS_TEXT)
   *   A=0 B=1 -> Y=1  (TOS_TEXT)
   *   A=1 B=1 -> Y=0  (TOS_TEXT_SEC)
   */
  struct {
    const char *label;
    const char *value;
  } tt_items[4] = {
      {
          " - A=0 B=0",
          "Y=1",
      },
      {
          " - A=1 B=0",
          "Y=1",
      },
      {
          " - A=0 B=1",
          "Y=1",
      },
      {
          " - A=1 B=1",
          "Y=0",
      },
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 5;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 5) % 5;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title("DEMO");

        PD_SetFont(FONT_ASCII_16);
        int cy = 33;

        /* Item 0: 00 Return */
        draw_card(0, sel, cy, "00 Return");

        /* Items 1-4: NAND truth table rows */
        for (int i = 0; i < 4; i++) {
          cy += 25;
          draw_card_r(i + 1, sel, cy, tt_items[i].label, tt_items[i].value);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

/* ── 03 Logic Test sub-page (scrollable menu, key_test pattern) ── */
static void hc00n_test_subpage(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 6;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 6) % 6;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
    }
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();
      uint8_t outputs = boardHC00N.readOutputByte();

      LCD_FLUSH({
        draw_frame_title("DEMO");

        PD_SetFont(FONT_ASCII_16);
        int cy = 33;

        /* Item 0: 00 Return */
        draw_card(0, sel, cy, "00 Return");

        /* Items 1-4: CH1 - CH4 */
        for (int ch = 0; ch < 4; ch++) {
          cy += 25;
          uint8_t state = (outputs >> (3 - ch)) & 0x01;

          char label[8];
          snprintf(label, sizeof(label), " - CH%d", ch + 1);

          const char *val = state ? "LOW" : "HIGH";

          draw_card_r(ch + 1, sel, cy, label, val);
        }

        /* Item 5: Value <0x00> */
        cy += 25;
        char hexbuf[8];
        snprintf(hexbuf, sizeof(hexbuf), "0x%02X", outputs);
        draw_card_r(5, sel, cy, " - Value", hexbuf);

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

/* ── Main activity (standard menu loop, key_test pattern) ── */
#define HC00N_N 4
void hc00n_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  boardHC00N.init();

  const char *items[HC00N_N] = {"00 Return", "01 Monitor", "02 Truth Table",
                                "03 Logic Test"};

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % HC00N_N;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + HC00N_N) % HC00N_N;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0:
        return;
      case 1:
        hc00n_monitor_subpage();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 2:
        hc00n_truth_subpage();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      case 3:
        hc00n_test_subpage();
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        break;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        draw_frame_title("DEMO");
        PD_SetFont(FONT_ASCII_16);
        int vis = HC00N_N < 7 ? HC00N_N : 7;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > HC00N_N)
          start = HC00N_N - vis;
        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= HC00N_N)
            break;
          int cy = 33 + i * 25;
          draw_card(idx, sel, cy, items[idx]);
        }
        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}
