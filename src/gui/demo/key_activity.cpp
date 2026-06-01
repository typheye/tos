#include "include/key_activity.hpp"
#include "core/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

/* ── Standard template functions (exact copy from about-page) ── */
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

#define KEY_N 14
#define KEY_VIS 7

void key_test_activity(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  const char *key_labels[KEY_N] = {
      "00 Return", "01 SW1",  "   SW2",  "   SW3",  "   SW4",
      "02 SW5",    "   SW6",  "   SW7",  "   SW8",  "   SW9",
      "03 SW10",   "   SW11", "   SW12", "   SW13",
  };

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % KEY_N;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + KEY_N) % KEY_N;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le)
      return;
    le = ce;

    if (HAL_GetTick() - lu > 200) {
      lu = HAL_GetTick();

      /* Read all states */
      bool s1 = keyManager.sw1_E0.isOn();
      bool s2 = keyManager.sw2_G13.isOn();
      bool s3 = keyManager.sw3_E2.isOn();
      bool s4 = keyManager.sw4_E4.isOn();
      bool s5 = keyManager.sw5_D6.isOn();
      bool s6 = keyManager.sw6_G9.isOn();
      bool s7 = keyManager.sw7_G11.isOn();
      bool s8 = keyManager.sw8_G10.isOn();
      bool s9 = keyManager.sw9_G15.isOn();
      bool s10 = keyManager.sw10_G3.isOn();
      bool s11 = keyManager.sw11_D15.isOn();
      bool s12 = keyManager.sw12_B12.isOn();
      bool s13 = keyManager.sw13_B14.isOn();

      /* Build value strings & colors – ON=TOS_TEXT, OFF=TOS_TEXT_SEC */
      char key_vals[KEY_N][8];
      key_vals[0][0] = '\0';

      // Group 1: SW1-SW4 (normal logic)
      snprintf(key_vals[1], 8, "%s", s1 ? "ON" : "OFF");
      snprintf(key_vals[2], 8, "%s", s2 ? "ON" : "OFF");
      snprintf(key_vals[3], 8, "%s", s3 ? "ON" : "OFF");
      snprintf(key_vals[4], 8, "%s", s4 ? "ON" : "OFF");

      // Group 2: SW5-SW9 (inverted logic – true=OFF, false=ON)
      snprintf(key_vals[5], 8, "%s", s5 ? "ON" : "OFF");
      snprintf(key_vals[6], 8, "%s", s6 ? "ON" : "OFF");
      snprintf(key_vals[7], 8, "%s", s7 ? "ON" : "OFF");
      snprintf(key_vals[8], 8, "%s", s8 ? "ON" : "OFF");
      snprintf(key_vals[9], 8, "%s", s9 ? "ON" : "OFF");

      // Group 3: SW10-SW13 (normal logic)
      snprintf(key_vals[10], 8, "%s", s10 ? "ON" : "OFF");
      snprintf(key_vals[11], 8, "%s", s11 ? "ON" : "OFF");
      snprintf(key_vals[12], 8, "%s", s12 ? "ON" : "OFF");
      snprintf(key_vals[13], 8, "%s", s13 ? "ON" : "OFF");

      LCD_FLUSH({
        draw_frame_title("DEMO");
        PD_SetFont(FONT_ASCII_16);

        int vis = KEY_VIS;
        int start = sel - vis / 2;
        if (start < 0)
          start = 0;
        if (start + vis > KEY_N)
          start = KEY_N - vis;

        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= KEY_N)
            break;
          int cy = 33 + i * 25;
          if (idx == 0)
            draw_card(idx, sel, cy, key_labels[idx]);
          else
            draw_card_r(idx, sel, cy, key_labels[idx], key_vals[idx]);
        }

        PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}
