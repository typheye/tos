#include "demo/include/esp8266_activity.hpp"
#include "hardware/include/esp8266.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include <stdio.h>
#include <string.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}

static const char *esp_menus[] = {
    "01 Init", "02 Connect WiFi",
    "03 HTTP GET", "04 Back"};

static void draw_menu(int select) {
  PD_FillScreen(TOS_BG);
  bar("08");

  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < 4; i++) {
    int cy = 33 + i * 25;
    if (i == select) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, esp_menus[i]);
  }

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(ESP8266_IsConnected() ? TOS_GREEN : TOS_GREY);
  PD_DrawString(16, 160, ESP8266_IsConnected() ? "Status: Connected" : "Status: Disconnected");

  bbar("ENTER", NULL, "UP/DOWN");
  LCD_Flush();
}

static void show_message(const char *line1, const char *line2, uint32_t color) {
  PD_FillScreen(TOS_BG);
  bar("08");

  PD_DrawAngledCard(8, 44, 224, 60, 6, TOS_CARD_BG);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(color);
  PD_DrawString(16, 54, line1);
  if (line2) PD_DrawString(16, 74, line2);

  bbar("EXIT", NULL, NULL);
  LCD_Flush();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
}

static void init_esp8266(void) {
  printf("\r\n========== Init ESP8266 ==========\r\n");
  char at_cmd[] = "AT\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)at_cmd, 4, 1000);
  HAL_Delay(500);
  ESP8266_Init();
  show_message("ESP8266 Initialized", "Check serial monitor", LV_TEXT_PRIMARY);
}

static void connect_wifi(void) {
  const char *ssid = "JanPNP One 0000";
  const char *password = "admin123";
  printf("\r\n========== Connect WiFi ==========\r\n");
  printf("Connecting to: %s\r\n", ssid);

  if (ESP8266_ConnectWiFi(ssid, password)) {
    show_message("WiFi Connected!", ssid, LV_SUCCESS);
  } else {
    show_message("WiFi Connection Failed!", "Check SSID/Password", LV_ERROR);
  }
}

static void http_get_test(void) {
  printf("\r\n========== HTTP GET Test ==========\r\n");
  if (!ESP8266_IsConnected()) {
    show_message("WiFi not connected!", "Please connect first", LV_ERROR);
    return;
  }

  if (!ESP8266_StartTCP("baidu.com", 80)) {
    show_message("TCP connection failed!", NULL, LV_ERROR);
    return;
  }

  const char *request = "GET /get HTTP/1.1\r\n"
                        "Host: baidu.com\r\n"
                        "Connection: close\r\n\r\n";
  if (ESP8266_SendData((const uint8_t *)request, strlen(request))) {
    printf("Request sent!\r\n");
    show_message("HTTP Request Sent!", "Check Serial Monitor", LV_SUCCESS);
  } else {
    show_message("HTTP Request Failed!", NULL, LV_ERROR);
  }
  HAL_Delay(3000);
}

void esp8266_test_activity(void) {
  int menu_select = 0;
  uint8_t le = 0;
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++; if (menu_select >= 4) menu_select = 3;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (menu_select) {
      case 0: init_esp8266(); break;
      case 1: connect_wifi(); break;
      case 2: http_get_test(); break;
      case 3: return;
      }
    }
    le = ce;
    draw_menu(menu_select);
    HAL_Delay(50);
  }
}

void esp8266_scan_activity(void) {
  printf("\r\n========== WiFi Scan ==========\r\n");
  ESP8266_SendCommand("AT+CWLAP", "OK", 10000);
  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
}

void esp8266_tcp_server_test(void) {
  printf("\r\n========== TCP Server Test ==========\r\n");
  ESP8266_SendCommand("AT+CIPMUX=1", "OK", 1000);
  if (ESP8266_SendCommand("AT+CIPSERVER=1,8888", "OK", 2000)) {
    printf("Server started on port 8888\r\n");
    show_message("TCP Server Running", "Port: 8888", LV_SUCCESS);
    ESP8266_SendCommand("AT+CIPSERVER=0", "OK", 1000);
    ESP8266_SendCommand("AT+CIPMUX=0", "OK", 1000);
  }
}
