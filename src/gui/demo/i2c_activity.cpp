/**
 ******************************************************************************
 * @file    i2c_activity.cpp
 * @author  Typheye
 * @brief   I2C Activity implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "include/i2c_activity.hpp"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern I2C_HandleTypeDef hi2c1;

/* ==================================================================
 *  Known devices lookup table
 * ================================================================== */

typedef struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  const char *name;
} KnownDevice_t;

static const KnownDevice_t known_devices[] = {
    {0x50, 0xA0, "JY901S"},  {0x77, 0xEE, "BMP180"},
    {0x68, 0xD0, "MPU6050"}, {0x3C, 0x78, "OLED SSD1306"},
    {0x57, 0xAE, "Camera"},  {0x1E, 0x3C, "HMC5883L"},
    {0x68, 0xD0, "DS3231"},  {0x29, 0x52, "TCS3472"}};

#define KNOWN_COUNT (sizeof(known_devices) / sizeof(known_devices[0]))

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

/* ==================================================================
 *  Scan state & results (CCMRAM)
 * ================================================================== */

enum { SCAN_IDLE = 0, SCAN_SCANNING = 1, SCAN_DONE = 2 };

static CCMRAM struct {
  uint8_t addr_7bit;
  uint8_t addr_8bit;
  const char *known_name;
} scan_results[112];
static CCMRAM uint32_t result_count = 0;
static CCMRAM uint8_t scan_state = SCAN_IDLE;

static const char *get_known_device_name(uint8_t addr_7bit) {
  for (uint32_t i = 0; i < KNOWN_COUNT; i++) {
    if (known_devices[i].addr_7bit == addr_7bit)
      return known_devices[i].name;
  }
  return NULL;
}

/* ==================================================================
 *  Draw helpers
 * ================================================================== */

static void draw_progress(int x, int y, int w, int h, int pct) {
  PD_SetColor(TOS_CARD_BG);
  PD_SetFill(true);
  PD_DrawRect(x, y, w, h);
  int fill_w = pct > 0 ? (w - 4) * pct / 100 : 0;
  if (fill_w > 0) {
    PD_SetColor(TOS_ACCENT);
    PD_DrawRect(x + 2, y + 2, fill_w, h - 4);
  }
  PD_SetFill(false);
  PD_SetColor(TOS_GREY);
  PD_DrawRect(x, y, w, h);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT);
  char buf[16];
  snprintf(buf, sizeof(buf), "Scan: %d%%", pct);
  PD_DrawStringCentered(x, y, w, h, buf);
}

/* ==================================================================
 *  I2C scan (blocking, with live progress)
 * ================================================================== */

static void do_i2c_scan(void) {
  result_count = 0;
  scan_state = SCAN_SCANNING;
  uint32_t last_lcd = 0;

  for (int addr = 0x08; addr <= 0x77; addr++) {
    uint8_t addr_8bit = addr << 1;

    if (HAL_I2C_IsDeviceReady(&hi2c1, addr_8bit, 3, 20) == HAL_OK) {
      scan_results[result_count].addr_7bit = addr;
      scan_results[result_count].addr_8bit = addr_8bit;
      scan_results[result_count].known_name = get_known_device_name(addr);
      result_count++;
    }
    JPDelay(2);

    /* Throttled progress update */
    if (HAL_GetTick() - last_lcd > 100 || addr == 0x77) {
      last_lcd = HAL_GetTick();
      int pct = (addr - 0x08 + 1) * 100 / (0x77 - 0x08 + 1);
      LCD_FLUSH({
        UI_DrawFrameTitle("DEMO");
        PD_SetFont(FONT_ASCII_16);
        PD_SetColor(TOS_TEXT);
        PD_DrawString(16, 33, "Scanning I2C bus...");
        draw_progress(16, 55, 208, 24, pct);
      });
    }
  }

  scan_state = SCAN_DONE;
}

/* ==================================================================
 *  Results page
 * ================================================================== */

static void draw_results_page(int sel) {
  LCD_FLUSH({
    UI_DrawFrameTitle("DEMO");
    int n = 1 + (int)result_count;
    int visible = n < 7 ? n : 7;
    int start = sel - visible / 2;
    if (start < 0)
      start = 0;
    if (start + visible > n)
      start = n - visible;
    if (start < 0)
      start = 0;

    PD_SetFont(FONT_ASCII_16);
    for (int i = 0; i < visible; i++) {
      int idx = start + i;
      if (idx >= n)
        break;
      int cy = 33 + i * 25;

      if (idx == 0) {
        UI_DrawMenuCard(idx, sel, cy, "00 Return");
      } else {
        int dev_idx = idx - 1;
        char buf[36];
        snprintf(
            buf, sizeof(buf), " - 0x%02X %s", scan_results[dev_idx].addr_7bit,
            scan_results[dev_idx].known_name ? scan_results[dev_idx].known_name
                                             : "Unknown");
        UI_DrawMenuCard(idx, sel, cy, buf);
      }
    }
    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

/* ==================================================================
 *  Main entry point
 * ================================================================== */

void i2c_scan_activity_gui(void) {
  uint32_t lu = 0;

  /* 1. Show initial loading screen */
  LCD_FLUSH({
    UI_DrawFrameTitle("DEMO");
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(16, 33, "Scanning I2C bus...");
  });

  /* 2. Perform scan (blocking, updates screen with progress) */
  do_i2c_scan();

  /* 3. No devices found */
  if (result_count == 0) {
    LCD_FLUSH({
      UI_DrawFrameTitle("DEMO");
      PD_SetFont(FONT_ASCII_16);
      PD_SetColor(TOS_RED);
      PD_DrawString(26, 33, "No I2C devices found!");
      PD_DrawFooterCenter("ENTER", NULL, NULL);
    });
    alert_show("ALERT", "No devices found!\nCheck connections.");
    return;
  }

  /* 4. Results page scrollable device list */
  int sel = 1;
  uint8_t le = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    int n = 1 + (int)result_count;
    if (sel >= n)
      sel = n - 1;

    /* UP (collision_A8) */
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % n;
      JPDelay(45);
    }
    /* DOWN (collision_D0) */
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + n) % n;
      JPDelay(45);
    }

    /* ENTER */
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return; /* "00 Return" -> exit activity */
      int dev_idx = sel - 1;
      if (dev_idx >= 0 && dev_idx < (int)result_count) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Device: %s\nAddress: 0x%02X",
                 scan_results[dev_idx].known_name
                     ? scan_results[dev_idx].known_name
                     : "Unknown",
                 scan_results[dev_idx].addr_7bit);
        alert_show("ALERT", msg);
      }
    }
    le = ce;

    /* Hold both UP+DOWN for 700 ms -> exit */
    bool up = keyManager.collision_A8.isPressed();
    bool down = keyManager.collision_D0.isPressed();
    static uint32_t et = 0;
    static bool ea = false;
    if (up && down && !ea) {
      et = HAL_GetTick();
      ea = true;
    } else if (up && down && ea) {
      if (HAL_GetTick() - et > 700)
        return;
    } else if (!up && !down) {
      ea = false;
    }

    /* Redraw at ~10 fps */
    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      draw_results_page(sel);
    }
    JPDelay(1);
  }
}

void i2c_scan_activity(void) { i2c_scan_activity_gui(); }
