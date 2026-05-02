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

// 显示菜单
static void draw_menu(int select) {
  PD_FillScreen(LCD_COLOR_BLACK);

  // 标题
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "ESP8266 Test");

  // 菜单项
  const char *menus[] = {"1. Init ESP8266", "2. Connect WiFi",
                         "3. HTTP GET Test", "4. Back"};

  PD_SetFont(FONT_ASCII_12);
  for (int i = 0; i < 4; i++) {
    int y = 50 + i * 35;
    if (i == select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(10, y - 3, 220, 25);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }
    PD_DrawString(20, y, menus[i]);
  }

  // 状态显示
  PD_SetColor(LCD_COLOR_CYAN);
  if (ESP8266_IsConnected()) {
    PD_DrawString(10, 210, "Status: Connected");
  } else {
    PD_DrawString(10, 210, "Status: Disconnected");
  }

  // 底部提示
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 220, 240, 20);
  PD_SetFill(false);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(10, 224, "A8:Down D0:Up");
  PD_DrawString(130, 224, "Enter:OK");

  LCD_Flush();
}

static void init_esp8266(void) {
  printf("\r\n========== Init ESP8266 ==========\r\n");

  // 先简单发送 AT 命令测试
  printf("Sending AT...\r\n");
  char at_cmd[] = "AT\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)at_cmd, 4, 1000);

  // 等待响应
  HAL_Delay(500);

  ESP8266_Init();
  printf("========== Init Complete ==========\r\n");

  PD_FillScreen(LCD_COLOR_BLACK);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetFont(FONT_ASCII_12);
  PD_DrawString(10, 30, "ESP8266 Initialized");
  PD_DrawString(10, 60, "Check serial monitor");
  PD_DrawString(10, 90, "Press Enter to return");
  LCD_Flush();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 连接 WiFi（需要用户输入）
static void connect_wifi(void) {
  printf("\r\n========== Connect WiFi ==========\r\n");
  printf("Please enter SSID and password via serial:\r\n");
  printf("Format: SSID,password\r\n");
  printf("Example: MyWiFi,12345678\r\n");
  printf("Or modify code to use fixed credentials.\r\n");

  // 临时使用固定凭据（请修改为你的 WiFi 信息）
  const char *ssid = "JanPNP One 0000"; // 修改为你的 WiFi SSID
  const char *password = "admin123";    // 修改为你的 WiFi 密码

  printf("Connecting to: ");
  printf(ssid);
  printf("\r\n");

  if (ESP8266_ConnectWiFi(ssid, password)) {
    printf("WiFi Connected!\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_GREEN);
    PD_SetFont(FONT_ASCII_12);
    PD_DrawString(10, 30, "WiFi Connected!");
    PD_DrawString(10, 60, "SSID: ");
    PD_DrawString(50, 60, ssid);
    PD_DrawString(10, 100, "Press Enter to return");
    LCD_Flush();
  } else {
    printf("WiFi Connection Failed!\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_RED);
    PD_DrawString(10, 30, "WiFi Failed!");
    PD_DrawString(10, 60, "Check SSID/Password");
    PD_DrawString(10, 100, "Press Enter to return");
    LCD_Flush();
  }

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// HTTP GET 测试
static void http_get_test(void) {
  printf("\r\n========== HTTP GET Test ==========\r\n");

  if (!ESP8266_IsConnected()) {
    printf("WiFi not connected! Please connect first.\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_RED);
    PD_DrawString(10, 30, "WiFi not connected!");
    PD_DrawString(10, 60, "Please connect first");
    PD_DrawString(10, 100, "Press Enter to return");
    LCD_Flush();

    while (1) {
      keyManager.btn_enter.tick();
      if (keyManager.btn_enter.getState() == KEY_PRESSED) {
        break;
      }
      HAL_Delay(50);
    }
    return;
  }

  // 连接到 baidu.com
  printf("Connecting to baidu.com:80...\r\n");
  if (!ESP8266_StartTCP("baidu.com", 80)) {
    printf("TCP connection failed!\r\n");
    return;
  }
  printf("TCP connected!\r\n");

  // 发送 HTTP GET 请求
  const char *request = "GET /get HTTP/1.1\r\n"
                        "Host: baidu.com\r\n"
                        "Connection: close\r\n"
                        "\r\n";

  printf("Sending HTTP request...\r\n");
  if (ESP8266_SendData((const uint8_t *)request, strlen(request))) {
    printf("Request sent! Response will arrive shortly.\r\n");
    printf("Check serial monitor for response.\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_GREEN);
    PD_DrawString(10, 30, "HTTP Request Sent!");
    PD_DrawString(10, 60, "Check Serial Monitor");
    PD_DrawString(10, 100, "Press Enter to return");
    LCD_Flush();
  } else {
    printf("Failed to send request!\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_RED);
    PD_DrawString(10, 30, "HTTP Request Failed!");
    PD_DrawString(10, 100, "Press Enter to return");
    LCD_Flush();
  }

  // 等待几秒让数据接收完成
  HAL_Delay(3000);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 主测试函数
void esp8266_test_activity(void) {
  int menu_select = 0;
  uint8_t last_enter_state = 0;

  boardLCD.fillScreen(LCD_COLOR_BLACK);
  printf("\r\n========== ESP8266 Test Menu ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 菜单导航
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= 4)
        menu_select = 3;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0)
        menu_select--;
      HAL_Delay(150);
    }

    // 执行操作
    uint8_t current_enter =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter == 1 && last_enter_state == 0) {
      switch (menu_select) {
      case 0:
        init_esp8266();
        break;
      case 1:
        connect_wifi();
        break;
      case 2:
        http_get_test();
        break;
      case 3:
        printf("Exit ESP8266 Test\r\n");
        return;
      }
    }
    last_enter_state = current_enter;

    draw_menu(menu_select);
    HAL_Delay(50);
  }
}

// 简化的扫描测试（使用 AT+CWLAP）
void esp8266_scan_activity(void) {
  printf("\r\n========== WiFi Scan ==========\r\n");
  printf("Sending AT+CWLAP command...\r\n");
  printf("Check serial monitor for results.\r\n");

  if (ESP8266_SendCommand("AT+CWLAP", "OK", 10000)) {
    printf("Scan completed\r\n");
  } else {
    printf("Scan failed\r\n");
  }

  // 等待按键退出
  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }
    HAL_Delay(50);
  }
}

// 简化的 TCP 服务器测试
void esp8266_tcp_server_test(void) {
  printf("\r\n========== TCP Server Test ==========\r\n");
  printf("Starting TCP server on port 8888...\r\n");

  // 设置多连接模式
  ESP8266_SendCommand("AT+CIPMUX=1", "OK", 1000);

  // 创建服务器
  if (ESP8266_SendCommand("AT+CIPSERVER=1,8888", "OK", 2000)) {
    printf("Server started on port 8888\r\n");
    printf("Use telnet to connect\r\n");
    printf("Press Enter to stop...\r\n");

    PD_FillScreen(LCD_COLOR_BLACK);
    PD_SetColor(LCD_COLOR_GREEN);
    PD_DrawString(10, 30, "TCP Server Running");
    PD_DrawString(10, 60, "Port: 8888");
    PD_DrawString(10, 100, "Press Enter to stop");
    LCD_Flush();

    // 等待退出
    while (1) {
      keyManager.btn_enter.tick();
      if (keyManager.btn_enter.getState() == KEY_PRESSED) {
        break;
      }
      HAL_Delay(50);
    }

    // 关闭服务器
    ESP8266_SendCommand("AT+CIPSERVER=0", "OK", 1000);
    ESP8266_SendCommand("AT+CIPMUX=0", "OK", 1000);
    printf("Server stopped\r\n");
  } else {
    printf("Failed to start server\r\n");
  }
}