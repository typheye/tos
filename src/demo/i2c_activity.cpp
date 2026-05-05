#include "include/i2c_activity.hpp"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "main.h"
#include <stdio.h>

extern USART boardSerial;
extern JY901S boardJY901S;
extern BMP180 boardBMP180;
extern KeyManager keyManager;
extern I2C_HandleTypeDef hi2c1;
extern LCD boardLCD;

// 已知设备列表
typedef struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  const char *name;
} KnownDevice_t;

static const KnownDevice_t known_devices[] = {
    {0x50, 0xA0, "JY901S"},        {0x77, 0xEE, "BMP180"},
    {0x68, 0xD0, "MPU6050"},       {0x3C, 0x78, "OLED SSD1306"},
    {0x57, 0xAE, "Camera OV2640"}, {0x1E, 0x3C, "HMC5883L"},
    {0x68, 0xD0, "DS3231 RTC"},    {0x29, 0x52, "TCS3472"}};

#define KNOWN_COUNT (sizeof(known_devices) / sizeof(known_devices[0]))

// 扫描结果存储
static struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  int found;
  const char *known_name;
} scan_results[112]; // 0x08-0x77 共 112 个地址
static uint32_t result_count = 0;
static int scan_in_progress = 0;
static int scan_complete = 0;
static int current_scan_addr = 0x08;

// 清除指定区域
// static void clear_area(int x, int y, int w, int h) {
//   PD_SetColor(LCD_COLOR_BLACK);
//   PD_SetFill(true);
//   PD_DrawRect(x, y, w, h);
//   PD_SetFill(false);
// }

// 获取已知设备名称
static const char *get_known_device_name(uint8_t addr_7bit) {
  for (uint32_t i = 0; i < KNOWN_COUNT; i++) {
    if (known_devices[i].addr_7bit == addr_7bit) {
      return known_devices[i].name;
    }
  }
  return NULL;
}

// 执行 I2C 扫描
static void perform_i2c_scan(void) {
  result_count = 0;
  current_scan_addr = 0x08;
  scan_in_progress = 1;
  scan_complete = 0;

  printf("\r\n========== I2C Scan Activity ==========\r\n");
  printf("Scanning I2C bus...\r\n");
}

// 扫描下一步（在主循环中调用）
static void scan_step(void) {
  if (!scan_in_progress)
    return;

  if (current_scan_addr <= 0x77) {
    uint8_t addr_8bit = current_scan_addr << 1;
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, addr_8bit, 3, 20);

    if (status == HAL_OK) {
      scan_results[result_count].addr_7bit = current_scan_addr;
      scan_results[result_count].addr_8bit = addr_8bit;
      scan_results[result_count].found = 1;
      scan_results[result_count].known_name =
          get_known_device_name(current_scan_addr);
      result_count++;

      char dbg[64];
      ;
      sprintf(dbg, "  Device found at 0x%02X (7-bit: 0x%02X)\r\n", addr_8bit,
              current_scan_addr);
      printf(dbg);

      if (scan_results[result_count - 1].known_name) {
        sprintf(dbg, "    -> %s\r\n",
                scan_results[result_count - 1].known_name);
        printf(dbg);
      }
    }

    current_scan_addr++;
    HAL_Delay(2);
  } else {
    // 扫描完成
    scan_in_progress = 0;
    scan_complete = 1;

    if (result_count == 0) {
      printf("  No I2C devices found!\r\n");
    } else {
      char dbg[64];
      ;
      sprintf(dbg, "  Total: %lu device(s) found\r\n", result_count);
      printf(dbg);
    }
    printf("========== Scan Complete ==========\r\n");
  }
}

// 绘制状态栏
static void draw_status_bar(void) {
  // 顶部栏背景
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  // 标题
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "I2C Scanner");
}

// 绘制扫描状态
static void draw_scan_status(void) {
  char dbg[32];
  int y_offset = 35;

  PD_SetFont(FONT_ASCII_12);

  if (scan_in_progress) {
    // 显示扫描进度
    PD_SetColor(LCD_COLOR_YELLOW);
    PD_DrawString(10, y_offset, "Scanning I2C bus...");

    int progress = ((current_scan_addr - 0x08) * 100) / (0x77 - 0x08 + 1);
    sprintf(dbg, "%d%%", progress);
    PD_SetColor(LCD_COLOR_CYAN);
    PD_DrawString(170, y_offset, dbg);

    // 显示当前扫描地址
    sprintf(dbg, "Addr: 0x%02X", current_scan_addr);
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(10, y_offset + 16, dbg);

  } else if (scan_complete) {
    // 扫描完成
    PD_SetColor(LCD_COLOR_GREEN);
    PD_DrawString(10, y_offset, "Scan Complete!");

    sprintf(dbg, "Found: %lu device(s)", result_count);
    PD_SetColor(result_count > 0 ? LCD_COLOR_YELLOW : LCD_COLOR_RED);
    PD_DrawString(10, y_offset + 16, dbg);
  } else {
    // 未开始扫描
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(10, y_offset, "Press ENTER to scan");
  }
}

// 绘制扫描结果
static void draw_scan_results(void) {
  int y_offset = 85;

  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetFont(FONT_ASCII_12);

  if (result_count > 0 && !scan_in_progress) {
    // 显示结果标题
    PD_SetColor(LCD_COLOR_YELLOW);
    PD_DrawString(10, y_offset - 12, "Found Devices:");

    // 显示地址
    char dbg[32];

    // 显示最多 5 个设备
    int max_display = (result_count < 5) ? result_count : 5;
    for (int32_t i = 0; i < max_display; i++) {
      y_offset += 16;

      sprintf(dbg, "0x%02X (0x%02X)", scan_results[i].addr_8bit,
              scan_results[i].addr_7bit);
      PD_SetColor(LCD_COLOR_CYAN);
      PD_DrawString(15, y_offset, dbg);

      // 显示设备名称
      if (scan_results[i].known_name) {
        PD_SetColor(LCD_COLOR_GREEN);
        PD_DrawString(100, y_offset, scan_results[i].known_name);
      } else {
        PD_SetColor(LCD_COLOR_GRAY);
        PD_DrawString(100, y_offset, "Unknown device");
      }
    }

    if (result_count > 5) {
      PD_SetColor(LCD_COLOR_GRAY);
      sprintf(dbg, "... and %lu more", result_count - 5);
      PD_DrawString(15, y_offset + 20, dbg);
    }

  } else if (scan_complete && result_count == 0 && !scan_in_progress) {
    PD_SetColor(LCD_COLOR_RED);
    PD_DrawString(25, y_offset, "No I2C devices found!");
    PD_DrawString(25, y_offset + 16, "Check connections");
  }
}

// 绘制底部提示
static void draw_bottom_bar(void) {
  // 底部栏背景
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 215, 240, 25);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);

  if (scan_in_progress) {
    PD_DrawString(10, 219, "Scanning...");
  } else {
    PD_DrawString(10, 219, "Enter: Scan");
  }
  PD_DrawString(130, 219, "Exit: Back");
}

// I2C 扫描 GUI 主函数
void i2c_scan_activity_gui(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  // 初始化 PD 图形库
  PD_Init();

  // 重置状态
  scan_in_progress = 0;
  scan_complete = 0;
  result_count = 0;
  current_scan_addr = 0x08;

  // 清屏
  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== I2C Scanner GUI Started ==========\r\n");

  // 主循环
  while (1) {
    // 更新按键状态
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 如果正在扫描，继续执行扫描步骤
    if (scan_in_progress) {
      scan_step();
    } else {
      // 检查 Enter 键 - 开始扫描
      uint8_t current_enter_state =
          (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
      if (current_enter_state == 1 && last_enter_state == 0) {
        printf("Starting I2C scan...\r\n");
        perform_i2c_scan();
      }
      last_enter_state = current_enter_state;
    }

    // 检查退出条件 - 碰撞开关 A8 或 D0 同时按下退出
    if (keyManager.collision_A8.getState() == KEY_PRESSED &&
        keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (scan_in_progress) {
        printf("Scan interrupted by user\r\n");
      }
      printf("Exit I2C Scanner GUI\r\n");
      break;
    }

    // 每 100ms 更新一次显示
    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      // 重新绘制所有内容
      PD_FillScreen(LCD_COLOR_BLACK);

      draw_status_bar();
      draw_scan_status();
      draw_scan_results();
      draw_bottom_bar();

      // 刷新屏幕
      LCD_Flush();
    }

    HAL_Delay(20);
  }
}

// 保持原有函数接口兼容
void i2c_scan_activity(void) { i2c_scan_activity_gui(); }