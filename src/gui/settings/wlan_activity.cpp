/**
 * @file    wlan_activity.cpp
 * @brief   WLAN Settings
 *
 * Menu:
 *   00 Return
 *   01 WLAN <ON/OFF>
 *      Scanning... / Connected
 *   02 Auto Connect <ON/OFF>
 *   03 Saved (N)
 */

#include "include/wlan_activity.hpp"
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/include/settings_manager.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "syslog.h"
#include "core/include/systime.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;
extern ESP8266 esp8266;

#define CCMRAM __attribute__((section(".ccmram")))
#define MAX_APS 20
#define SSID_LEN 24

static bool wlan_on        = false;
static bool wlan_auto_conn = false;
static bool wlan_connected = false;
static bool wlan_edit      = false;
static char wlan_ssid[SSID_LEN] = "";
static char wlan_pwd[32]   = "";

/* ==================================================================
 *  Helpers
 * ================================================================== */

static void wlan_copy(char *dst, size_t dst_sz, const char *src) {
  if (!dst || dst_sz == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dst_sz - 1);
  dst[dst_sz - 1] = '\0';
}

static void wlan_clear_state(void) {
  wlan_connected = false;
  wlan_ssid[0] = '\0';
  wlan_pwd[0]  = '\0';
  SM_Wlan_SetSSID("");
  SM_Wlan_SetPWD("");
}

static const char *trunc_str(const char *s, char *out, int maxw) {
  if (!s) s = "";
  int len = strlen(s);
  if (len <= maxw) { strcpy(out, s); return out; }
  memcpy(out, s, maxw - 3);
  out[maxw - 3] = '.'; out[maxw - 2] = '.'; out[maxw - 1] = '.';
  out[maxw] = '\0';
  return out;
}

static void wlan_load_state(void) {
  wlan_on        = SM_Wlan_On();
  wlan_auto_conn = SM_Wlan_AutoConn();
  wlan_connected = esp8266.isConnected();
  if (wlan_connected) {
    wlan_copy(wlan_ssid, sizeof(wlan_ssid), SM_Wlan_SSID());
    wlan_copy(wlan_pwd, sizeof(wlan_pwd), SM_Wlan_PWD());
  }
}

/* ==================================================================
 *  WiFi scan
 * ================================================================== */

static CCMRAM char ap_ssid[MAX_APS][SSID_LEN];
static CCMRAM int  ap_rssi[MAX_APS];
static CCMRAM int  ap_enc [MAX_APS];
static int  ap_count = 0;

static bool do_scan(void) {
  ap_count = 0;
  LOG_I("WLAN", "Setting STA mode and scanning...");
  ESP8266_SendCommand("AT+CWMODE=1", "OK", 3000);
  HAL_Delay(200);
  LOG_I("WLAN", "Scanning with AT+CWLAP...");
  if (!esp8266.scanNetworks()) {
    LOG_W("WLAN", "Scan failed, retrying once...");
    HAL_Delay(300);
    if (!esp8266.scanNetworks()) {
      LOG_E("WLAN", "Scan failed after retry");
      alert_show("ALERT", "WiFi scan failed. Check module.");
      return false;
    }
  }
  const char *buf = esp8266.getRxBuffer();
  if (!buf || !*buf) {
    alert_show("ALERT", "No WiFi data received.");
    return false;
  }
  LOG_D("WLAN", "Raw %d bytes", (int)strlen(buf));
  const char *p = buf;
  while (p && *p && ap_count < MAX_APS) {
    p = strstr(p, "+CWLAP:");
    if (!p) break;
    p += 7;
    int ecn = 0;
    if (*p == '(') p++;
    ecn = (int)(*p - '0');
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
    if (*p == '"') p++;
    int si = 0;
    while (*p && *p != '"' && si < SSID_LEN - 1)
      ap_ssid[ap_count][si++] = *p++;
    ap_ssid[ap_count][si] = '\0';
    if (si == 0) strcpy(ap_ssid[ap_count], "N/A");
    if (*p == '"') p++;
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
    int rssi = 0; bool neg = false;
    if (*p == '-') { neg = true; p++; }
    while (*p >= '0' && *p <= '9') { rssi = rssi * 10 + (*p - '0'); p++; }
    if (neg) rssi = -rssi;
    if (si == 0 || strcmp(ap_ssid[ap_count], "N/A") == 0) continue;
    bool dup = false;
    for (int d = 0; d < ap_count; d++) {
      if (strcmp(ap_ssid[d], ap_ssid[ap_count]) == 0) {
        if (rssi > ap_rssi[d]) { ap_rssi[d] = rssi; ap_enc[d] = ecn; }
        dup = true; break;
      }
    }
    if (!dup) {
      /* Filter out already-saved SSIDs */
      if (SM_Saved_Find(ap_ssid[ap_count])) continue;
      ap_enc[ap_count] = ecn;
      ap_rssi[ap_count] = rssi;
      LOG_D("WLAN", "%d: \"%s\" RSSI=%d enc=%d", ap_count, ap_ssid[ap_count], rssi, ecn);
      ap_count++;
    }
  }
  LOG_I("WLAN", "%d unique networks", ap_count);
  return true;
}

/* ==================================================================
 *  Draw helpers
 * ================================================================== */

static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  extern TRTC boardTRTC;
  static uint32_t last_tm = 0;
  if (HAL_GetTick() - last_tm > 1000) {
    last_tm = HAL_GetTick();
    Time_t t; Date_t d;
    boardTRTC.getDateTime(&t, &d);
    char ts[8]; time_fmt(ts, sizeof(ts), t.hours, t.minutes);
    PD_SetHeaderTime(ts);
  }
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c  = selected ? TOS_TEXT : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2) card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label,
                        const char *value, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c  = selected ? TOS_TEXT : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2) card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

static void draw_signal_bars(int x, int y, int card_h, int rssi) {
  int bars = (rssi >= -50) ? 4 : (rssi >= -60) ? 3 : (rssi >= -70) ? 2 : (rssi >= -80) ? 1 : 0;
  uint32_t active = (bars >= 3) ? TOS_GREEN : (bars >= 1) ? TOS_YELLOW : TOS_RED;
  int bar_w = 4, gap = 1, total_w = 4*bar_w+3*gap, bx = x-total_w;
  int max_h = card_h-6, base_y = y+card_h/2+max_h/2;
  PD_SetFill(true);
  for (int b = 0; b < 4; b++) {
    int bh = 3+b*3, px = bx+b*(bar_w+gap);
    PD_SetColor(TOS_GREY); PD_DrawRect(px, base_y-bh, bar_w, bh);
    if (b < bars) { PD_SetColor(active); PD_DrawRect(px, base_y-bh, bar_w, bh); }
  }
  PD_SetFill(false);
}

/* ==================================================================
 *  Scanning page
 * ================================================================== */

static void draw_scaning(int sel) {
  draw_frame_title("WLAN");
  int n = 2 + ap_count;
  int visible = n < 7 ? n : 7;
  int start = sel - visible/2;
  if (start < 0) start = 0;
  if (start+visible > n) start = n-visible;
  if (start < 0) start = 0;
  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < visible; i++) {
    int idx = start+i; if (idx >= n) break;
    int cy = 33+i*25;
    if (idx == 0) { draw_card(idx, sel, cy, "00 Return", false); }
    else if (idx == 1) { draw_card(idx, sel, cy, "01 Refresh", false); }
    else {
      int ap_idx = idx-2;
      bool selected = (idx == sel);
      uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
      uint32_t txt_c = TOS_TEXT;
      PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
      char sec = (ap_enc[ap_idx] == 0) ? 'O' : 'L';
      char disp[28]; trunc_str(ap_ssid[ap_idx], disp, 16);
      char line[32]; snprintf(line, sizeof(line), " %c %s", sec, disp);
      PD_SetColor(txt_c); PD_DrawString(26, cy+2, line);
      draw_signal_bars(222, cy, 20, ap_rssi[ap_idx]);
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN"); LCD_Flush();
}

static void scaning_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  draw_frame_title("WLAN");
  PD_SetColor(TOS_TEXT); PD_DrawString(26, 33, "Scanning WiFi..."); LCD_Flush();
  do_scan();

  int sel = 1; uint8_t le = 0; uint32_t lu = 0;
  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    int n = 2+ap_count; if (sel >= n) sel = n-1;

    if (keyManager.collision_A8.getState() == KEY_PRESSED)
    { sel = (sel+1)%n; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED)
    { sel = (sel-1+n)%n; HAL_Delay(150); }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0) return;
      if (sel == 1) {
        boardLCD.fillScreen(LCD_COLOR_BLACK); draw_frame_title("WLAN");
        PD_SetColor(TOS_TEXT); PD_DrawString(26, 33, "Scanning WiFi..."); LCD_Flush();
        do_scan(); sel = 1; lu = 0;
      } else if (sel >= 2) {
        int ap_idx = sel-2;
        char title[40]; snprintf(title, sizeof(title), "Password for %s", ap_ssid[ap_idx]);
        char pwd[32] = "";
        if (keyboard_open(title, pwd, 31)) {
          LOG_I("WLAN", "Connecting to %s...", ap_ssid[ap_idx]);
          boardLCD.fillScreen(LCD_COLOR_BLACK); draw_frame_title("WLAN");
          PD_SetColor(TOS_TEXT); PD_DrawString(26, 33, "Connecting..."); LCD_Flush();
          bool ok = ESP8266_ConnectWiFi(ap_ssid[ap_idx], pwd);
          HAL_Delay(500);
          if (ok && ESP8266_IsConnected()) {
            wlan_connected = true;
            wlan_copy(wlan_ssid, sizeof(wlan_ssid), ap_ssid[ap_idx]);
            wlan_copy(wlan_pwd, sizeof(wlan_pwd), pwd);
            SM_Wlan_SetSSID(wlan_ssid);
            SM_Wlan_SetPWD(wlan_pwd);
            SM_Saved_Add(wlan_ssid, wlan_pwd);
            LOG_I("WLAN", "Connected to %s", wlan_ssid);
            alert_show("ALERT", "WiFi connected successfully!");
            return;
          } else {
            wlan_connected = false;
            LOG_E("WLAN", "Connection failed");
            alert_show("ALERT", "Connection failed. Check password.");
          }
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK); draw_frame_title("WLAN");
        PD_SetColor(TOS_TEXT); PD_DrawString(26, 33, "Scanning WiFi..."); LCD_Flush();
        do_scan();
      }
    }
    le = ce;

    bool up = keyManager.collision_A8.isPressed(), down = keyManager.collision_D0.isPressed();
    static uint32_t et = 0; static bool ea = false;
    if (up && down && !ea) { et = HAL_GetTick(); ea = true; }
    else if (up && down && ea) { if (HAL_GetTick()-et > 700) return; }
    else if (!up && !down) { ea = false; }

    if (HAL_GetTick()-lu > 100) { lu = HAL_GetTick(); draw_scaning(sel); }
    HAL_Delay(20);
  }
}

/* ==================================================================
 *  Connected sub-page
 * ================================================================== */

static void connected_detail_popup(void) {
  char ssid_t[28], pwd_t[28];
  trunc_str(wlan_ssid, ssid_t, 24);
  trunc_str(wlan_pwd,  pwd_t,  24);
  char msg[96];
  snprintf(msg, sizeof(msg), "SSID:\n%s\nPassword:\n%s", ssid_t, pwd_t);
  alert_show("WLAN", msg);
}

static void connected_page(void) {
  int sel = 0; uint8_t le = 0; uint32_t lu = 0;
  const char *items[] = {"00 Return", "01 Detail", "02 Disconnect"};
  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED)
    { sel = (sel+1)%3; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED)
    { sel = (sel-1+3)%3; HAL_Delay(150); }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0: return;
      case 1: connected_detail_popup(); break;
      case 2:
        ESP8266_Disconnect();
        wlan_clear_state();
        alert_show("OK", "Disconnected");
        return;
      }
    }
    le = ce;

    if (HAL_GetTick()-lu > 100) {
      lu = HAL_GetTick();
      draw_frame_title("WLAN");
      PD_SetFont(FONT_ASCII_16);
      for (int i = 0; i < 3; i++) {
        int cy = 33+i*25;
        draw_card(i, sel, cy, items[i], false);
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN"); LCD_Flush();
    }
    HAL_Delay(20);
  }
}

/* ==================================================================
 *  Saved networks
 * ================================================================== */

static bool saved_did_action = false; /* signal to jump back to main */

static void saved_net_action(int idx) {
  const SM_SavedNet_t *net = SM_Saved_Get(idx);
  if (!net) return;
  bool is_current = wlan_connected && (strcmp(wlan_ssid, net->ssid) == 0);
  int sel = 0; uint8_t le = 0; uint32_t lu = 0;
  const char *items[] = {"00 Return", "01 Detail", "02 Connect", "03 Forget"};

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED)
    { sel = (sel+1)%4; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED)
    { sel = (sel-1+4)%4; HAL_Delay(150); }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sel) {
      case 0: return;
      case 1: {
        char ss[28], pw[28];
        trunc_str(net->ssid, ss, 24);
        trunc_str(net->pwd,  pw, 24);
        char m[96]; snprintf(m, sizeof(m), "SSID:\n%s\nPassword:\n%s", ss, pw);
        alert_show("WLAN", m);
        break;
      }
      case 2:
        if (is_current) { alert_show("WLAN", "Already connected"); break; }
        /* Show connecting screen */
        boardLCD.fillScreen(LCD_COLOR_BLACK); draw_frame_title("WLAN");
        PD_SetColor(TOS_TEXT); PD_DrawString(26, 33, "Connecting..."); LCD_Flush();
        if (ESP8266_ConnectWiFi(net->ssid, net->pwd)) {
          HAL_Delay(500);
          if (ESP8266_IsConnected()) {
            wlan_connected = true;
            wlan_copy(wlan_ssid, sizeof(wlan_ssid), net->ssid);
            wlan_copy(wlan_pwd, sizeof(wlan_pwd), net->pwd);
            SM_Wlan_SetSSID(net->ssid); SM_Wlan_SetPWD(net->pwd);
            SM_Saved_Add(net->ssid, net->pwd);
            alert_show("OK", "Connected!");
            saved_did_action = true;
            return;
          }
        }
        alert_show("FAIL", "Connection failed"); break;
      case 3:
        SM_Saved_Del(idx);
        alert_show("OK", "Forgotten");
        saved_did_action = true;
        return;
      }
    }
    le = ce;

    if (HAL_GetTick()-lu > 100) { lu = HAL_GetTick();
      draw_frame_title("WLAN"); PD_SetFont(FONT_ASCII_16);
      for (int i = 0; i < 4; i++) {
        if (i == 2 && is_current) {
          /* Grey out "02 Connect" — currently connected to this network */
          bool s = (i == sel);
          uint32_t cc = s ? TOS_ACCENT : TOS_CARD_BG;
          PD_DrawAngledCard(14, 33+i*25, 212, 20, 5, cc);
          PD_SetColor(TOS_GREY);
          PD_DrawString(26, 33+i*25+2, items[i]);
        } else {
          draw_card(i, sel, 33+i*25, items[i], false);
        }
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN"); LCD_Flush(); }
    HAL_Delay(20);
  }
}

static void saved_networks_page(void) {
  int count = SM_Saved_Count();
  int total = count+1, sel = 0; uint8_t le = 0; uint32_t lu = 0;
  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED)
    { sel = (sel+1)%total; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED)
    { sel = (sel-1+total)%total; HAL_Delay(150); }
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0) return;
      saved_did_action = false;
      saved_net_action(sel-1);
      if (saved_did_action) return; /* jump back to main */
      count = SM_Saved_Count(); total = count+1; if (sel >= total) sel = total-1;
    }
    le = ce;
    if (HAL_GetTick()-lu > 100) { lu = HAL_GetTick();
      draw_frame_title("WLAN"); PD_SetFont(FONT_ASCII_16);
      for (int i = 0; i < total; i++) {
        int cy = 33+i*25;
        if (i == 0) draw_card(0, sel, cy, "00 Return", false);
        else { const SM_SavedNet_t *sn = SM_Saved_Get(i-1);
               char b[40]; snprintf(b, sizeof(b), " L %s", sn ? sn->ssid : "?");
               draw_card(i, sel, cy, b, false); }
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN"); LCD_Flush(); }
    HAL_Delay(20);
  }
}

/* ==================================================================
 *  Main WLAN page
 * ================================================================== */

static int wlan_item_count(void) {
  int n = 2;
  if (wlan_on) { n++; n++; n++; }
  return n;
}

static void draw_wlan_main(int sel) {
  draw_frame_title("WLAN");
  PD_SetFont(FONT_ASCII_16);
  int n = wlan_item_count();
  int vis = n < 7 ? n : 7;
  int start = sel - vis/2;
  if (start < 0) start = 0;
  if (start+vis > n) start = n-vis;

  for (int i = 0; i < vis; i++) {
    int idx = start+i; if (idx >= n) break;
    int cy = 33+i*25;

    if (idx == 0) {
      draw_card(idx, sel, cy, "00 Return", false);
    } else if (idx == 1) {
      char buf[32]; snprintf(buf, sizeof(buf), "01 WLAN");
      draw_card_r(idx, sel, cy, buf, wlan_on?"ON":"OFF", wlan_edit);
    } else if (idx == 2 && wlan_on) {
      bool s = (idx == sel);
      uint32_t card_c = s ? TOS_ACCENT : TOS_CARD_BG;
      PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
      PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
      PD_DrawString(26, cy+2, wlan_connected ? "   Connected" : "   Scanning");
    } else if (idx == 3 && wlan_on) {
      char buf[32]; snprintf(buf, sizeof(buf), "02 Auto Connect");
      draw_card_r(idx, sel, cy, buf, wlan_auto_conn?"ON":"OFF",
                  wlan_edit && (idx == 3));
    } else if (idx == 4 && wlan_on) {
      char buf[32]; snprintf(buf, sizeof(buf), "03 Saved (%u)", SM_Saved_Count());
      draw_card(idx, sel, cy, buf, false);
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN"); LCD_Flush();
}

static int wlan_main_loop(void) {
  static int sel = 0;
  uint8_t le = 0; uint32_t lu = 0;
  if (sel >= wlan_item_count()) sel = wlan_item_count()-1;
  wlan_edit = false;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (wlan_edit) {
        if (sel == 1) { wlan_on = !wlan_on; SM_Wlan_SetOn(wlan_on);
          if (!wlan_on) { ESP8266_Disconnect(); wlan_clear_state(); }
        }
        if (sel == 3 && wlan_on) { wlan_auto_conn = !wlan_auto_conn; SM_Wlan_SetAutoConn(wlan_auto_conn); }
      } else { sel = (sel+1) % wlan_item_count(); }
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (wlan_edit) {
        if (sel == 1) { wlan_on = !wlan_on; SM_Wlan_SetOn(wlan_on);
          if (!wlan_on) { ESP8266_Disconnect(); wlan_clear_state(); }
        }
        if (sel == 3 && wlan_on) { wlan_auto_conn = !wlan_auto_conn; SM_Wlan_SetAutoConn(wlan_auto_conn); }
      } else { sel = (sel-1+wlan_item_count()) % wlan_item_count(); }
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (wlan_edit) {
        wlan_edit = false;
        LOG_I("WLAN", "Set %s", wlan_on?"ON":"OFF");
      } else if (sel == 0) {
        return 0;
      } else if (sel == 1) {
        wlan_edit = true;
      } else if (wlan_on) {
        if (sel == 2) {
          if (wlan_connected) connected_page(); else scaning_run();
          wlan_load_state();
        } else if (sel == 3) {
          wlan_edit = true;
        } else if (sel == 4) {
          saved_networks_page();
        }
      }
    }
    le = ce;

    if (HAL_GetTick()-lu > 100) {
      lu = HAL_GetTick();
      wlan_load_state();
      draw_wlan_main(sel);
    }
    HAL_Delay(20);
  }
}

/* ==================================================================
 *  Public API
 * ================================================================== */

void wlan_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  wlan_load_state();
  while (1) {
    int act = wlan_main_loop();
    if (act == 0) return;
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  }
}

/* Status icon helpers */
extern "C" bool esp_wlan_is_connected(void) { return esp8266.isConnected(); }
