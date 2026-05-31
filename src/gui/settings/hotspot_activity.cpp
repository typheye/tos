#include "include/hotspot_activity.hpp"
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/include/settings_manager.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include <cstdio>
#include <cstring>
#include "syslog.h"
#include "core/include/systime.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern ESP8266 esp8266;

static bool hs_on = false;
static bool hs_edit = false;
static char hs_ssid[24] = "TOS-Hotspot";
static char hs_pwd[32] = "12345678";
static char ap_ip[24] = "";

// (CWLIF not supported on this ESP8266 firmware)

// ============ Draw helpers ============

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

static void draw_card(int idx, int sel, int cy, const char *text,
                      bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c = selected ? TOS_TEXT : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2)
    card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c = selected ? TOS_TEXT : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2)
    card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

// ============ Hotspot ON/OFF ============

static void hs_start(void) {
  LOG_I("HOTS", "Starting hotspot: %s", hs_ssid);
  ESP8266_SendCommand("AT+CWMODE=2", "OK", 3000); // softAP mode
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",6,3", hs_ssid, hs_pwd);
  ESP8266_SendCommand(cmd, "OK", 5000);
  ESP8266_SendCommand("AT+CIPMUX=1", "OK", 2000);
  ESP8266_SendCommand("AT+CIPSERVER=1,80", "OK", 3000);
  // ESP8266 softAP default gateway is always 192.168.4.1
  strcpy(ap_ip, "192.168.4.1");
  LOG_I("HOTS", "Hotspot started, AP IP: %s", ap_ip);
}

static void hs_stop(void) {
  LOG_I("HOTS", "Stopping hotspot");
  ESP8266_SendCommand("AT+CIPSERVER=0", "OK", 2000);
  ESP8266_SendCommand("AT+CIPMUX=0", "OK", 2000);
  ESP8266_SendCommand("AT+CWMODE=1", "OK", 2000); // back to STA mode
  ap_ip[0] = '\0';
  LOG_I("HOTS", "Hotspot stopped");
}

// CWLIF not supported on this firmware — show hotspot info instead

// ============ Main menu ============

static int hs_item_count(void) {
  int n = 2; // Return + toggle
  if (hs_on)
    n++; // SSID & Password
  return n;
}

static void draw_hs_main(int sel) {
  draw_frame_title("HOTS");
  PD_SetFont(FONT_ASCII_16);
  int n = hs_item_count();
  int visible = n < 7 ? n : 7;
  int start = sel - visible / 2;
  if (start < 0)
    start = 0;
  if (start + visible > n)
    start = n - visible;

  for (int i = 0; i < visible; i++) {
    int idx = start + i;
    if (idx >= n)
      break;
    int cy = 33 + i * 25;
    switch (idx) {
    case 0:
      draw_card(idx, sel, cy, "00 Return", false);
      break;
    case 1: {
      char b[32];
      snprintf(b, sizeof(b), "01 Hotspot");
      draw_card_r(idx, sel, cy, b, hs_on ? "ON" : "OFF", hs_edit);
      break;
    }
    case 2:
      draw_card(idx, sel, cy, "02 SSID & Password", false);
      break;
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static int hs_main_loop(void) {
  static int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;
  if (sel >= hs_item_count())
    sel = hs_item_count() - 1;
  hs_edit = false;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (hs_edit)
        hs_on = !hs_on;
      else
        sel = (sel + 1) % hs_item_count();
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (hs_edit)
        hs_on = !hs_on;
      else
        sel = (sel - 1 + hs_item_count()) % hs_item_count();
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (hs_edit) {
        hs_edit = false;
        if (hs_on)
          hs_start();
        else
          hs_stop();
        LOG_I("HOTS", "Set %s", hs_on ? "ON" : "OFF");
      } else if (sel == 0) {
        return 0;
      } else if (sel == 1) {
        hs_edit = true;
      } else if (hs_on && sel == 2) {
        return 2; // SSID & Password
      }
    }
    le = ce;
    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_hs_main(sel);
    }
    HAL_Delay(20);
  }
}

// ============ SSID & Password page ============

static void draw_ssidpwd(int sel) {
  draw_frame_title("HOTS");
  PD_SetFont(FONT_ASCII_16);

  draw_card(0, sel, 33, "00 Return", false);
  draw_card(1, sel, 58, "01 Edit SSID", false);
  draw_card(2, sel, 83, "02 Edit Password", false);

  PD_SetColor(TOS_TEXT_SEC);
  char buf[48];
  snprintf(buf, sizeof(buf), "SSID:%s", hs_ssid);
  PD_DrawString(26, 115, buf);
  snprintf(buf, sizeof(buf), "PWD: %s", hs_pwd);
  PD_DrawString(26, 135, buf);

  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static void ssidpwd_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 3;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 3) % 3;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
      if (sel == 1) {
        keyboard_open("SSID", hs_ssid, 23);
        if (hs_ssid[0] == '\0')
          strcpy(hs_ssid, "TOS-Hotspot");
        SM_Hotspot_SetSSID(hs_ssid);
        if (hs_on)
          hs_start();
      }
      if (sel == 2) {
        keyboard_open("Password", hs_pwd, 31);
        if (hs_pwd[0] == '\0')
          strcpy(hs_pwd, "12345678");
        SM_Hotspot_SetPWD(hs_pwd);
        if (hs_on)
          hs_start();
      }
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    le = ce;
    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_ssidpwd(sel);
    }
    HAL_Delay(20);
  }
}

// ============ Public ============

void hotspot_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  while (1) {
    int act = hs_main_loop();
    if (act == 0)
      return;
    if (act == 2) {
      ssidpwd_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}

/* Status icon helper */
extern "C" bool hotspot_is_active(void) { return hs_on; }
