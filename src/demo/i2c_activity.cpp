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

typedef struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  const char *name;
} KnownDevice_t;

static const KnownDevice_t known_devices[] = {
    {0x50, 0xA0, "JY901S"},   {0x77, 0xEE, "BMP180"},
    {0x68, 0xD0, "MPU6050"},  {0x3C, 0x78, "OLED SSD1306"},
    {0x57, 0xAE, "Camera"},   {0x1E, 0x3C, "HMC5883L"},
    {0x68, 0xD0, "DS3231"},   {0x29, 0x52, "TCS3472"}};

#define KNOWN_COUNT (sizeof(known_devices) / sizeof(known_devices[0]))

static struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  int found;
  const char *known_name;
} scan_results[112];
static uint32_t result_count = 0;
static int scan_in_progress = 0;
static int scan_complete = 0;
static int current_scan_addr = 0x08;

static const char *get_known_device_name(uint8_t addr_7bit) {
  for (uint32_t i = 0; i < KNOWN_COUNT; i++) {
    if (known_devices[i].addr_7bit == addr_7bit) return known_devices[i].name;
  }
  return NULL;
}

static void perform_i2c_scan(void) {
  result_count = 0;
  current_scan_addr = 0x08;
  scan_in_progress = 1;
  scan_complete = 0;
}

static void scan_step(void) {
  if (!scan_in_progress) return;
  if (current_scan_addr <= 0x77) {
    uint8_t addr_8bit = current_scan_addr << 1;
    HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(&hi2c1, addr_8bit, 3, 20);
    if (status == HAL_OK) {
      scan_results[result_count].addr_7bit = current_scan_addr;
      scan_results[result_count].addr_8bit = addr_8bit;
      scan_results[result_count].found = 1;
      scan_results[result_count].known_name = get_known_device_name(current_scan_addr);
      result_count++;
    }
    current_scan_addr++;
    HAL_Delay(2);
  } else {
    scan_in_progress = 0;
    scan_complete = 1;
  }
}

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}

static void draw_scan_status(void) {
  char dbg[32];
  PD_SetFont(FONT_ASCII_16);
  if (scan_in_progress) {
    PD_SetColor(LV_WARNING);
    PD_DrawString(16, 50, "Scanning I2C bus...");
    int progress = ((current_scan_addr - 0x08) * 100) / (0x77 - 0x08 + 1);
    sprintf(dbg, "%d%%", progress);
    PD_SetColor(LV_ACCENT);
    PD_DrawString(190, 50, dbg);
    sprintf(dbg, "Addr: 0x%02X", current_scan_addr);
    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, 72, dbg);
  } else if (scan_complete) {
    PD_SetColor(LV_SUCCESS);
    PD_DrawString(16, 50, "Scan Complete!");
    sprintf(dbg, "Found: %lu device(s)", result_count);
    PD_SetColor(result_count > 0 ? LV_WARNING : LV_ERROR);
    PD_DrawString(16, 72, dbg);
  } else {
    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, 50, "Press ENTER to scan");
  }
}

static void draw_scan_results(void) {
  int y_offset = 100;
  PD_SetFont(FONT_ASCII_16);
  if (result_count > 0 && !scan_in_progress) {
    PD_DrawAngledCard(8, y_offset - 8, 224, 110, 6, TOS_CARD_BG);
    PD_SetColor(LV_WARNING);
    PD_DrawString(16, y_offset, "Found Devices:");

    int max_display = (result_count < 5) ? result_count : 5;
    for (int32_t i = 0; i < max_display; i++) {
      char dbg[32];
      sprintf(dbg, "0x%02X", scan_results[i].addr_7bit);
      PD_SetColor(LV_ACCENT);
      PD_DrawString(24, y_offset + 20 + i * 18, dbg);
      if (scan_results[i].known_name) {
        PD_SetColor(LV_SUCCESS);
        PD_DrawString(80, y_offset + 20 + i * 18, scan_results[i].known_name);
      } else {
        PD_SetColor(LV_TEXT_HINT);
        PD_DrawString(80, y_offset + 20 + i * 18, "Unknown");
      }
    }
    if (result_count > 5) {
      PD_SetColor(LV_TEXT_HINT);
      char dbg[32];
      sprintf(dbg, "... and %lu more", result_count - 5);
      PD_DrawString(24, y_offset + 20 + max_display * 18 + 8, dbg);
    }
  } else if (scan_complete && result_count == 0 && !scan_in_progress) {
    PD_SetColor(LV_ERROR);
    PD_DrawString(30, y_offset, "No I2C devices found!");
    PD_DrawString(30, y_offset + 20, "Check connections");
  }
}

void i2c_scan_activity_gui(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();
  PD_Init();

  scan_in_progress = 0;
  scan_complete = 0;
  result_count = 0;
  current_scan_addr = 0x08;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (scan_in_progress) {
      scan_step();
    } else {
      uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
      if (ce && !le) {
        perform_i2c_scan();
      }
      le = ce;
    }

    if (keyManager.collision_A8.getState() == KEY_PRESSED &&
        keyManager.collision_D0.getState() == KEY_PRESSED) {
      break;
    }

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("03");
      draw_scan_status();
      draw_scan_results();
      if (scan_in_progress)
        bbar("Scanning...", NULL, "UP/DOWN");
      else
        bbar("EXIT", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}

void i2c_scan_activity(void) { i2c_scan_activity_gui(); }
