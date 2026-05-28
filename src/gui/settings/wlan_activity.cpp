#include "wlan_activity.hpp"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/libpd.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;
extern ESP8266 esp8266;

static bool wlan_on        = false;
static bool wlan_connected = false;
static bool wlan_edit      = false;

// ============ WiFi scan results ============

#define MAX_APS  20
#define SSID_LEN 24
static char ap_ssid[MAX_APS][SSID_LEN];
static int  ap_rssi[MAX_APS];
static int  ap_enc[MAX_APS];
static int  ap_count = 0;

static void do_scan(void) {
  ap_count = 0;
  printf("[WLAN] Warming up ESP8266...\r\n");
  ESP8266_SendCommand("AT", "OK", 1000);  // ensure module is responsive
  HAL_Delay(100);
  printf("[WLAN] Scanning with AT+CWLAP...\r\n");
  if (!esp8266.scanNetworks()) {
    printf("[WLAN] Scan failed, retrying once...\r\n");
    HAL_Delay(200);
    if (!esp8266.scanNetworks()) { printf("[WLAN] Scan failed\r\n"); return; }
  }
  const char *buf = esp8266.getRxBuffer();
  if (!buf || !*buf) { printf("[WLAN] Empty buffer\r\n"); return; }

  printf("[WLAN] Raw %d bytes\r\n", (int)strlen(buf));
  const char *p = buf;
  while (p && *p && ap_count < MAX_APS) {
    p = strstr(p, "+CWLAP:");
    if (!p) break; p += 7;
    int ecn = 0;
    if (*p == '(') p++; ecn = (int)(*p - '0');
    while (*p && *p != ',') p++; if (*p == ',') p++;
    if (*p == '"') p++;
    int si = 0;
    while (*p && *p != '"' && si < SSID_LEN - 1) ap_ssid[ap_count][si++] = *p++;
    ap_ssid[ap_count][si] = '\0';
    // Empty or unparseable → "N/A"
    if (si == 0) strcpy(ap_ssid[ap_count], "N/A");
    if (*p == '"') p++;
    while (*p && *p != ',') p++; if (*p == ',') p++;
    int rssi = 0; bool neg = false;
    if (*p == '-') { neg = true; p++; }
    while (*p >= '0' && *p <= '9') { rssi = rssi * 10 + (*p - '0'); p++; }
    if (neg) rssi = -rssi;
    ap_enc[ap_count] = ecn; ap_rssi[ap_count] = rssi;
    printf("[WLAN] %d: \"%s\" RSSI=%d enc=%d\r\n", ap_count, ap_ssid[ap_count], rssi, ecn);
    ap_count++;
  }
  printf("[WLAN] %d networks found\r\n", ap_count);
}

// ============ Draw helpers ============

static void draw_frame_title(const char *title) {
  PD_Init();
  PD_FillScreen(TOS_BG);
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, title);
}

static void draw_card(int idx, int sel, int cy, const char *text, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c  = selected ? TOS_TEXT   : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2) card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, text);
}

static void draw_card_r(int idx, int sel, int cy, const char *label, const char *value, bool editing) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c  = selected ? TOS_TEXT   : TOS_TEXT_SEC;
  if (editing && selected && (HAL_GetTick() / 300) % 2) card_c = TOS_CARD_BG;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, label);
  uint16_t vw = PD_GetStringWidth(value);
  PD_DrawString(220 - vw, cy + 2, value);
}

// Signal bars: vertical centre, pushed right, dark grey background for missing
static void draw_signal_bars(int x, int y, int card_h, int rssi) {
  int bars = (rssi >= -50) ? 4 : (rssi >= -60) ? 3 : (rssi >= -70) ? 2 : (rssi >= -80) ? 1 : 0;
  uint32_t active = (bars >= 3) ? TOS_GREEN : (bars >= 1) ? TOS_YELLOW : TOS_RED;
  int bar_w = 4, gap = 1;
  int total_w = 4 * bar_w + 3 * gap;  // ~19px
  int bx = x - total_w;
  int max_h = card_h - 6;              // tallest bar ~14px
  int base_y = y + card_h / 2 + max_h / 2;

  PD_SetFill(true);
  for (int b = 0; b < 4; b++) {
    int bh = 3 + b * 3;  // bar heights: 3,6,9,12
    int px = bx + b * (bar_w + gap);
    PD_SetColor(TOS_GREY);
    PD_DrawRect(px, base_y - bh, bar_w, bh);
    if (b < bars) {
      PD_SetColor(active);
      PD_DrawRect(px, base_y - bh, bar_w, bh);
    }
  }
  PD_SetFill(false);
}

// ============ Main WLAN menu ============

static int wlan_item_count(void) {
  int n = 2;  // Return + WLAN toggle
  if (wlan_on) {
    n++;  // Scaning
    n++;  // Status
    if (wlan_connected) n++;  // Disconnect (only when connected)
  }
  return n;
}

static void draw_wlan_main(int sel) {
  draw_frame_title("WLAN");

  PD_SetFont(FONT_ASCII_16);
  int n = wlan_item_count();
  int visible = n < 7 ? n : 7;
  int start = sel - visible / 2;
  if (start < 0) start = 0;
  if (start + visible > n) start = n - visible;

  for (int i = 0; i < visible; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int cy = 33 + i * 25;

    // Map logical index → item type
    int item = idx;
    if (idx >= 2 && wlan_on) {
      if (idx == 2) item = 2;       // Scaning
      else if (idx == 3) item = 3;  // Status
      else if (idx == 4) item = 4;  // Disconnect
    }

    switch (item) {
    case 0:
      draw_card(idx, sel, cy, "00 Return", false);
      break;
    case 1: {
      char buf[32]; snprintf(buf, sizeof(buf), "01 WLAN");
      draw_card_r(idx, sel, cy, buf, wlan_on ? "ON" : "OFF", wlan_edit);
      break;
    }
    case 2:
      draw_card(idx, sel, cy, "02 Scaning", false);
      break;
    case 3:
      draw_card(idx, sel, cy, "03 Status", false);
      break;
    case 4:
      draw_card(idx, sel, cy, "04 Disconnct", false);
      break;
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static int wlan_main_loop(void) {
  int sel = 0; uint8_t le = 0; uint32_t lu = 0;
  wlan_edit = false;

  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (wlan_edit) { wlan_on = !wlan_on; }
      else { sel = (sel + 1) % wlan_item_count(); }
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (wlan_edit) { wlan_on = !wlan_on; }
      else { sel = (sel - 1 + wlan_item_count()) % wlan_item_count(); }
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (wlan_edit) {
        wlan_edit = false;
        printf("[WLAN] Set %s\r\n", wlan_on ? "ON" : "OFF");
      } else if (sel == 0) {
        return 0;
      } else if (sel == 1) {
        wlan_edit = true;
      } else {
        // Map selected index to action
        int n = wlan_item_count();
        if (wlan_on && sel >= 2) {
          int sub = sel - 2;
          if (sub == 0) return 2;                          // Scaning
          if (sub == 1) return 3;                          // Status
          if (sub == 2 && wlan_connected) {                // Disconnect
            ESP8266_SendCommand("AT+CWQAP", "OK", 3000);
            wlan_connected = false;
            sel = 0;
          }
        }
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick(); draw_wlan_main(sel); }
    HAL_Delay(20);
  }
}

// ============ Scaning sub-page ============

static void draw_scaning(int sel) {
  draw_frame_title("WLAN");

  int n = 2 + ap_count;
  int visible = n < 7 ? n : 7;
  int start = sel - visible / 2;
  if (start < 0) start = 0;
  if (start + visible > n) start = n - visible;
  if (start < 0) start = 0;

  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < visible; i++) {
    int idx = start + i;
    if (idx >= n) break;
    int cy = 33 + i * 25;

    if (idx == 0) {
      draw_card(idx, sel, cy, "00 Return", false);
    } else if (idx == 1) {
      draw_card(idx, sel, cy, "01 Refresh", false);
    } else {
      int ap_idx = idx - 2;
      bool selected = (idx == sel);
      uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
      uint32_t txt_c  = selected ? TOS_TEXT   : (ap_rssi[ap_idx] >= -60 ? TOS_TEXT : TOS_TEXT_SEC);
      PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);

      // Security + space + SSID
      char sec = (ap_enc[ap_idx] == 0) ? 'O' : 'L';
      char disp[28]; int sl = strlen(ap_ssid[ap_idx]);
      if (sl > 16) { memcpy(disp, ap_ssid[ap_idx], 13); disp[13]='.';disp[14]='.';disp[15]='\0'; }
      else strcpy(disp, ap_ssid[ap_idx]);
      char line[32]; snprintf(line, sizeof(line), " %c %s", sec, disp);

      PD_SetColor(txt_c);
      PD_DrawString(26, cy + 2, line);

      // Compact signal bars
      draw_signal_bars(222, cy, 20, ap_rssi[ap_idx]);
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static void scaning_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  draw_frame_title("WLAN");
  PD_SetColor(TOS_TEXT);
  PD_DrawString(40, 100, "Scanning WiFi...");
  LCD_Flush();
  do_scan();

  int sel = 0; uint8_t le = 0; uint32_t lu = 0;
  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();
    int n = 2 + ap_count;

    if (keyManager.collision_A8.getState() == KEY_PRESSED) { sel = (sel + 1) % n; HAL_Delay(150); }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) { sel = (sel - 1 + n) % n; HAL_Delay(150); }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0) return;
      if (sel == 1) {
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        draw_frame_title("WLAN");
        PD_SetColor(TOS_TEXT); PD_DrawString(40, 100, "Scanning WiFi...");
        LCD_Flush();
        do_scan(); sel = 0;
      }
    }
    le = ce;

    bool up = keyManager.collision_A8.isPressed(), down = keyManager.collision_D0.isPressed();
    static uint32_t et = 0; static bool ea = false;
    if (up && down && !ea) { et = HAL_GetTick(); ea = true; }
    else if (up && down && ea) { if (HAL_GetTick() - et > 700) return; }
    else if (!up && !down) { ea = false; }

    if (HAL_GetTick() - lu > 100) { lu = HAL_GetTick(); draw_scaning(sel); }
    HAL_Delay(20);
  }
}

// ============ Status sub-page ============

static void status_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  int sel = 0; uint8_t le = 0; uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick(); keyManager.collision_D0.tick(); keyManager.btn_enter.tick();
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le && sel == 0) return;
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_frame_title("WLAN");
      PD_SetFont(FONT_ASCII_16);
      draw_card(0, 0, 28, "00 Return", false);
      PD_SetColor(TOS_TEXT_SEC);
      PD_DrawString(26, 58, "Status: Not connected");
      PD_DrawFooterCenter("ENTER", NULL, NULL);
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}

// ============ Public API ============

void wlan_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  while (1) {
    int act = wlan_main_loop();
    if (act == 0) return;
    if (act == 2) { scaning_run(); boardLCD.fillScreen(LCD_COLOR_BLACK); }
    if (act == 3) { status_run();  boardLCD.fillScreen(LCD_COLOR_BLACK); }
  }
}
