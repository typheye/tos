#include "include/hotspot_activity.hpp"
#include "components/include/alert.hpp"
#include "components/include/keyboard.hpp"
#include "core/include/settings_manager.h"
#include "core/include/systime.h"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "syslog.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;
extern ESP8266 esp8266;
extern UART_HandleTypeDef huart2;

extern "C" {
extern uint8_t esp8266_global_buffer[512];
extern uint16_t esp8266_global_index;
extern uint8_t esp8266_data_ready;
}

static bool hs_on = false;
static bool hs_share_wlan = false;
static bool hs_edit = false;
static char hs_ssid[24] = "";
static char hs_pwd[32] = "";
static char hs_ip[16] = "";
static char ap_ip[24] = "";

struct HsClient {
  char ip[16];
  char mac[18];
  bool tcp_only;
};

static HsClient hs_clients[4];
static int hs_client_count = 0;
static char hs_client_diag[24] = "Not scanned";

static const size_t HS_AT_RX_SIZE = 512;

static void hs_copy(char *dst, size_t dst_sz, const char *src) {
  if (!dst || dst_sz == 0)
    return;
  if (!src)
    src = "";
  strncpy(dst, src, dst_sz - 1);
  dst[dst_sz - 1] = '\0';
}

static char *hs_at_buf(void) { return (char *)esp8266_global_buffer; }

static void hs_at_reset(void) {
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  memset(esp8266_global_buffer, 0, HS_AT_RX_SIZE);
}

static void hs_uart_discard(uint32_t idle_ms) {
  uint32_t last_rx = HAL_GetTick();

  while (HAL_GetTick() - last_rx < idle_ms) {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      (void)(uint8_t)(huart2.Instance->DR & 0xFF);
      last_rx = HAL_GetTick();
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
    }
  }
}

static void hs_at_begin(void) {
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  hs_at_reset();
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  hs_uart_discard(20);
}

static void hs_at_end(void) {
  hs_uart_discard(2);
  esp8266_global_index = 0;
  esp8266_data_ready = 0;
  __HAL_UART_CLEAR_OREFLAG(&huart2);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void hs_at_append(char c, char *out, size_t out_sz, size_t *len) {
  if (*len + 1 < out_sz) {
    out[*len] = c;
    ++(*len);
    out[*len] = '\0';
  }
}

static bool hs_at_command(const char *cmd, const char *expected,
                          uint32_t timeout_ms, uint32_t settle_ms, char *out,
                          size_t out_sz) {
  char tx[128];
  int n = snprintf(tx, sizeof(tx), "%s\r\n", cmd);
  uint32_t start = HAL_GetTick();
  uint32_t last_rx = start;
  size_t len = 0;
  bool matched = false;
  bool failed = false;

  if (n <= 0 || n >= (int)sizeof(tx))
    return false;

  hs_at_begin();
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, (uint16_t)n, 1000);

  while (HAL_GetTick() - start < timeout_ms) {
    bool got = false;

    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) != RESET) {
      char c = (char)(huart2.Instance->DR & 0xFF);
      hs_at_append(c, out, out_sz, &len);
      got = true;
      last_rx = HAL_GetTick();
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE) != RESET) {
      __HAL_UART_CLEAR_OREFLAG(&huart2);
      last_rx = HAL_GetTick();
    }

    if (got) {
      if (expected && strstr(out, expected))
        matched = true;
      if (strstr(out, "ERROR") || strstr(out, "FAIL"))
        failed = true;
    }

    if ((matched || failed) && (HAL_GetTick() - last_rx >= settle_ms))
      break;
  }

  hs_at_end();
  return matched && !failed;
}

static void hs_log_response(const char *tag, const char *resp) {
  char summary[96];
  size_t j = 0;

  if (!resp)
    resp = "";
  for (size_t i = 0; resp[i] && j + 1 < sizeof(summary); ++i) {
    char c = resp[i];
    if (c == '\r' || c == '\n' || c == '\t')
      c = ' ';
    if ((unsigned char)c < 32 || (unsigned char)c > 126)
      c = '.';
    summary[j++] = c;
  }
  summary[j] = '\0';
  LOG_D("HOTS", "%s(%u): %s", tag, (unsigned)strlen(resp), summary);
}

static bool hs_ip_char(char c) { return (c >= '0' && c <= '9') || c == '.'; }

static void hs_copy_token(char *dst, size_t dst_sz, const char *begin,
                          const char *end) {
  while (begin < end &&
         (*begin == ' ' || *begin == '"' || *begin == '\r' || *begin == '\n')) {
    ++begin;
  }
  while (end > begin && (end[-1] == ' ' || end[-1] == '"' || end[-1] == '\r' ||
                         end[-1] == '\n')) {
    --end;
  }

  size_t n = (size_t)(end - begin);
  if (n >= dst_sz)
    n = dst_sz - 1;
  memcpy(dst, begin, n);
  dst[n] = '\0';
}

static bool hs_client_exists(const char *ip) {
  for (int i = 0; i < hs_client_count; ++i) {
    if (strcmp(hs_clients[i].ip, ip) == 0)
      return true;
  }
  return false;
}

static void hs_add_client(const char *ip, const char *mac, bool tcp_only) {
  if (!ip || !ip[0] ||
      hs_client_count >= (int)(sizeof(hs_clients) / sizeof(hs_clients[0]))) {
    return;
  }
  if (hs_client_exists(ip))
    return;

  hs_copy(hs_clients[hs_client_count].ip,
          sizeof(hs_clients[hs_client_count].ip), ip);
  hs_copy(hs_clients[hs_client_count].mac,
          sizeof(hs_clients[hs_client_count].mac), mac);
  hs_clients[hs_client_count].tcp_only = tcp_only;
  ++hs_client_count;
}

static void hs_parse_cwlif(const char *resp) {
  const char *p = resp;

  while (p && *p &&
         hs_client_count < (int)(sizeof(hs_clients) / sizeof(hs_clients[0]))) {
    char ip[16];
    char mac[18];
    const char *line_end = strpbrk(p, "\r\n");
    const char *line = p;
    const char *comma;

    if (!line_end)
      line_end = p + strlen(p);
    while (line < line_end && (*line == ' ' || *line == '\t'))
      ++line;
    if (strncmp(line, "+CWLIF:", 7) == 0)
      line += 7;

    comma = (const char *)memchr(line, ',', (size_t)(line_end - line));
    if (comma && line < comma && hs_ip_char(*line) &&
        (size_t)(comma - line) < sizeof(ip)) {
      hs_copy_token(ip, sizeof(ip), line, comma);
      hs_copy_token(mac, sizeof(mac), comma + 1, line_end);
      if (strchr(ip, '.') && mac[0])
        hs_add_client(ip, mac, false);
    }

    p = (*line_end) ? line_end + 1 : line_end;
  }
}

static void hs_parse_cipstatus(const char *resp) {
  const char *p = resp;

  while ((p = strstr(p, "+CIPSTATUS:")) != 0 &&
         hs_client_count < (int)(sizeof(hs_clients) / sizeof(hs_clients[0]))) {
    char ip[16];
    const char *line_end = strpbrk(p, "\r\n");
    const char *q = p;
    int quote_count = 0;
    const char *ip_begin = 0;
    const char *ip_end = 0;

    if (!line_end)
      line_end = p + strlen(p);
    while (q < line_end) {
      if (*q == '"') {
        ++quote_count;
        if (quote_count == 3)
          ip_begin = q + 1;
        else if (quote_count == 4) {
          ip_end = q;
          break;
        }
      }
      ++q;
    }

    if (ip_begin && ip_end && (size_t)(ip_end - ip_begin) < sizeof(ip)) {
      hs_copy_token(ip, sizeof(ip), ip_begin, ip_end);
      if (strchr(ip, '.'))
        hs_add_client(ip, "TCP session", true);
    }

    p = (*line_end) ? line_end + 1 : line_end;
  }
}

static void hs_refresh_clients(void) {
  char *buf = hs_at_buf();

  hs_client_count = 0;
  strcpy(hs_client_diag, "CWLIF");

  if (hs_at_command("AT+CWLIF", "OK", 2500, 80, buf, HS_AT_RX_SIZE)) {
    hs_log_response("CWLIF", buf);
    hs_parse_cwlif(buf);
  } else {
    hs_log_response("CWLIF fail", buf);
    strcpy(hs_client_diag, "CWLIF fail");
  }

  if (hs_client_count == 0 &&
      hs_at_command("AT+CIPSTATUS", "OK", 2500, 80, buf, HS_AT_RX_SIZE)) {
    hs_log_response("CIPSTATUS", buf);
    hs_parse_cipstatus(buf);
    if (hs_client_count > 0)
      strcpy(hs_client_diag, "TCP fallback");
  }

  /* Filter out clients without MAC (stale CIPSTATUS-only entries) */
  int real = 0;
  for (int i = 0; i < hs_client_count; i++) {
    if (hs_clients[i].mac[0] && !hs_clients[i].tcp_only) {
      if (real != i)
        hs_clients[real] = hs_clients[i];
      real++;
    }
  }
  hs_client_count = real;

  LOG_I("HOTS", "%d client(s) found", hs_client_count);
}

static bool hs_enable_softap_dhcp(void) {
  char *buf = hs_at_buf();

  if (hs_at_command("AT+CWDHCP=1,2", "OK", 2500, 80, buf, HS_AT_RX_SIZE)) {
    LOG_D("HOTS", "SoftAP DHCP enabled");
    return true;
  }

  hs_log_response("CWDHCP new fail", buf);
  if (hs_at_command("AT+CWDHCP=0,1", "OK", 2500, 80, buf, HS_AT_RX_SIZE)) {
    LOG_D("HOTS", "SoftAP DHCP enabled (legacy)");
    return true;
  }

  hs_log_response("CWDHCP legacy fail", buf);
  return false;
}

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
  HAL_Delay(200);
  if (!hs_enable_softap_dhcp()) {
    LOG_W("HOTS", "SoftAP DHCP enable failed; CWLIF may stay empty");
  }
  char cmd[96];
  if (strlen(hs_pwd) < 8)
    strcpy(hs_pwd, "12345678");
  snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",6,3,4,0", hs_ssid, hs_pwd);
  if (!ESP8266_SendCommand(cmd, "OK", 5000)) {
    snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",6,3", hs_ssid, hs_pwd);
    ESP8266_SendCommand(cmd, "OK", 5000);
  }
  hs_enable_softap_dhcp();
  /* Set AP IP address */
  {
    char ip_cmd[48];
    snprintf(ip_cmd, sizeof(ip_cmd), "AT+CIPAP=\"%s\"", hs_ip);
    ESP8266_SendCommand(ip_cmd, "OK", 3000);
  }
  ESP8266_SendCommand("AT+CIPMUX=1", "OK", 2000);
  ESP8266_SendCommand("AT+CIPSERVER=1,80", "OK", 3000);
  ESP8266_SendCommand("AT+CIPSTO=60", "OK", 2000);
  hs_copy(ap_ip, sizeof(ap_ip), hs_ip);
  hs_refresh_clients();
  LOG_I("HOTS", "Started, AP IP: %s", ap_ip);
}

static void hs_stop(void) {
  LOG_I("HOTS", "Stopping hotspot");
  ESP8266_SendCommand("AT+CIPSERVER=0", "OK", 2000);
  ESP8266_SendCommand("AT+CIPMUX=0", "OK", 2000);
  ESP8266_SendCommand("AT+CWMODE=1", "OK", 2000); // back to STA mode
  ap_ip[0] = '\0';
  hs_client_count = 0;
  strcpy(hs_client_diag, "Not scanned");
  LOG_I("HOTS", "Hotspot stopped");
}

// ============ Main menu ============

static int hs_item_count(void) {
  int n = 2; // Return + toggle
  if (hs_on) {
    n++;
    n++;
    n++;
    n++;
  } // ShareWLAN + SSID&PWD + IP + Connected
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
    case 2: {
      bool can_share = SM_Wlan_On() && ESP8266_IsConnected();
      if (can_share) {
        char b[32];
        snprintf(b, sizeof(b), "   Share WLAN");
        draw_card_r(idx, sel, cy, b, hs_share_wlan ? "ON" : "OFF",
                    hs_edit && (sel == 2));
      } else {
        /* Grey, no ON/OFF, no flash */
        bool s = (idx == sel);
        uint32_t cc = s ? TOS_ACCENT : TOS_CARD_BG;
        PD_DrawAngledCard(14, cy, 212, 20, 5, cc);
        PD_SetColor(TOS_GREY);
        PD_DrawString(26, cy + 2, "   Share WLAN");
      }
      break;
    }
    case 3:
      draw_card(idx, sel, cy, "02 SSID & Password", false);
      break;
    case 4: {
      char b[32];
      snprintf(b, sizeof(b), "03 IP");
      draw_card_r(idx, sel, cy, b, hs_ip, hs_edit && (idx == 4));
      break;
    }
    case 5: {
      char cb[32];
      snprintf(cb, sizeof(cb), "04 Connected (%d)", hs_client_count);
      draw_card(idx, sel, cy, cb, false);
      break;
    }
    }
  }
  PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static int hs_main_loop(void) {
  static int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0, lq = 0;
  if (sel >= hs_item_count())
    sel = hs_item_count() - 1;
  hs_edit = false;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      if (hs_edit) {
        if (sel == 1) {
          hs_on = !hs_on;
        } else if (sel == 2) {
          hs_share_wlan = !hs_share_wlan;
          SM_Hotspot_SetShareWlan(hs_share_wlan);
        }
      } else
        sel = (sel + 1) % hs_item_count();
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (hs_edit) {
        if (sel == 1) {
          hs_on = !hs_on;
        } else if (sel == 2) {
          hs_share_wlan = !hs_share_wlan;
          SM_Hotspot_SetShareWlan(hs_share_wlan);
        }
      } else
        sel = (sel - 1 + hs_item_count()) % hs_item_count();
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (hs_edit) {
        hs_edit = false;
        if (sel == 1) {
          if (hs_on)
            hs_start();
          else
            hs_stop();
          LOG_I("HOTS", "Set %s", hs_on ? "ON" : "OFF");
        }
      } else if (sel == 0) {
        return 0;
      } else if (sel == 1) {
        hs_edit = true;
      } else if (hs_on && sel == 2) {
        /* Only respond if WLAN is ON and connected */
        if (SM_Wlan_On() && ESP8266_IsConnected()) {
          hs_edit = true;
        }
      } else if (hs_on && sel == 3) {
        return 3; // SSID & Password
      } else if (hs_on && sel == 4) {
        /* Edit IP */
        keyboard_open("IP (192.168.x.1)", hs_ip, 15);
        /* Validate: must be 192.168.X.1 where X in 1-255 */
        int a, b, c, d;
        if (sscanf(hs_ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 && a == 192 &&
            b == 168 && c >= 1 && c <= 255 && d == 1) {
          SM_Hotspot_SetIP(hs_ip);
          if (hs_on) {
            hs_stop();
            hs_start();
          }
        } else {
          strcpy(hs_ip, "192.168.4.1");
          SM_Hotspot_SetIP(hs_ip);
          alert_show("HOTS", "Invalid IP! Reset to default");
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK);
      } else if (hs_on && sel == 5) {
        return 5; // Connected
      }
    }
    le = ce;
    /* Periodic client refresh */
    if (hs_on && HAL_GetTick() - lq > 3000) {
      lq = HAL_GetTick();
      hs_refresh_clients();
    }
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

// ============ Connected clients page ============

static void connected_page(void) {
  /* Loading screen */
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  draw_frame_title("HOTS");
  PD_SetColor(TOS_TEXT);
  PD_DrawString(26, 33, "Querying...");
  LCD_Flush();
  hs_refresh_clients();

  int n = hs_client_count + 1, sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;
  uint32_t lq = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % n;
      HAL_Delay(100);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + n) % n;
      HAL_Delay(100);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
      /* Show client detail */
      HsClient *c = &hs_clients[sel - 1];
      char msg[96];
      snprintf(msg, sizeof(msg), "MAC:\n%s\nIP:\n%s", c->mac[0] ? c->mac : "-",
               c->ip[0] ? c->ip : "-");
      alert_show("HOTS", msg);
    }
    le = ce;

    /* Periodic refresh every 2s */
    if (HAL_GetTick() - lq > 2000) {
      lq = HAL_GetTick();
      hs_refresh_clients();
      n = hs_client_count + 1;
      if (sel >= n)
        sel = n - 1;
    }

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_frame_title("HOTS");
      PD_SetFont(FONT_ASCII_16);
      int vis = n < 7 ? n : 7;
      int start = sel - vis / 2;
      if (start < 0)
        start = 0;
      if (start + vis > n)
        start = n - vis;

      for (int i = 0; i < vis; i++) {
        int idx = start + i;
        if (idx >= n)
          break;
        int cy = 33 + i * 25;
        if (idx == 0) {
          draw_card(0, sel, cy, "00 Return", false);
        } else {
          HsClient *c = &hs_clients[idx - 1];
          char b[40];
          snprintf(b, sizeof(b), " - %s", c->mac[0] ? c->mac : "-");
          draw_card(idx, sel, cy, b, false);
        }
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(10);
  }
}

// ============ Public ============

void hotspot_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  /* Load from Flash */
  hs_share_wlan = SM_Hotspot_ShareWlan();
  hs_copy(hs_ip, sizeof(hs_ip), SM_Hotspot_IP());
  hs_copy(hs_ssid, sizeof(hs_ssid), SM_Hotspot_SSID());
  hs_copy(hs_pwd, sizeof(hs_pwd), SM_Hotspot_PWD());
  if (!hs_ip[0])
    strcpy(hs_ip, "192.168.4.1");
  if (!hs_ssid[0])
    strcpy(hs_ssid, "TOS-Hotspot");
  if (!hs_pwd[0])
    strcpy(hs_pwd, "12345678");

  while (1) {
    int act = hs_main_loop();
    if (act == 0)
      return;
    if (act == 3) {
      ssidpwd_run();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
    if (act == 5) {
      connected_page();
      boardLCD.fillScreen(LCD_COLOR_BLACK);
    }
  }
}

/* Status icon helper */
extern "C" bool hotspot_is_active(void) { return hs_on; }
