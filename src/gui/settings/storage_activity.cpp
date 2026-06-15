/**
 ******************************************************************************
 * @file    storage_activity.cpp
 * @author  Typheye
 * @brief   Storage Activity implementation.
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

#include "include/storage_activity.hpp"
#include "library/include/libdly.h"

extern KeyManager keyManager;
extern LCD boardLCD;
extern TSDIO boardSDIO;

static bool sd_present = false;
static uint32_t sd_cap_kb = 0;
static int sd_used_pct = 0;
static uint32_t builtin_cap_kb = 1024; // 1MB flash
static int builtin_used_pct = 0;

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

static void draw_card(int idx, int sel, int cy, const char *text) {
  bool selected = (idx == sel);
  uint32_t card_c = selected ? TOS_ACCENT : TOS_CARD_BG;
  uint32_t txt_c = selected ? TOS_TEXT : TOS_TEXT_SEC;
  PD_DrawAngledCard(14, cy, 212, 20, 5, card_c);
  PD_SetColor(txt_c);
  PD_DrawString(26, cy + 2, text);
}

static void draw_progress(int x, int y, int w, int h, int pct,
                          const char *label) {
  if (pct < 0)
    pct = 0;
  if (pct > 16)
    pct = 100;
  PD_SetColor(TOS_CARD_BG);
  PD_SetFill(true);
  PD_DrawRect(x, y, w, h);
  int fill_w = pct > 0 ? (w - 4) * pct / 100 : 0;
  if (fill_w > 0) {
    PD_SetColor(TOS_ACCENT);
    PD_DrawRect(x + 2, y + 2, fill_w, h - 4);
  }
  PD_SetFill(false);
  PD_SetColor(LV_BORDER);
  PD_DrawRect(x, y, w, h);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(TOS_TEXT);
  char buf[16];
  snprintf(buf, sizeof(buf), "%s: %d%%", label, pct);
  PD_DrawStringCentered(x, y, w, h, buf);
}

static const char *fmt_size(uint32_t kb) {
  static char buf[16];
  if (kb >= 1048576)
    snprintf(buf, sizeof(buf), "%luG", (unsigned long)(kb / 1048576));
  else if (kb >= 1024)
    snprintf(buf, sizeof(buf), "%luM", (unsigned long)(kb / 1024));
  else
    snprintf(buf, sizeof(buf), "%luK", (unsigned long)kb);
  return buf;
}

// ============ Get free % from FATFS ============

static bool get_sd_fs_info(int *used_pct, uint32_t *total_kb) {
  *used_pct = 0;
  *total_kb = 0;

  const char *path = "0:";
  FATFS fs;
  bool need_unmount = false;

  DWORD free_clusters;
  FATFS *fs_ptr;
  FRESULT res = f_getfree(path, &free_clusters, &fs_ptr);

  if (res != FR_OK) {
    res = f_mount(&fs, path, 1);
    if (res != FR_OK) {
      SysHandle_FatalFResult(res, SYS_ERR_UI_STORAGE_PROBE);
      return false;
    }
    need_unmount = true;
    res = f_getfree(path, &free_clusters, &fs_ptr);
  }

  if (res != FR_OK) {
    if (need_unmount)
      f_mount(NULL, path, 0);
    SysHandle_FatalFResult(res, SYS_ERR_UI_STORAGE_PROBE);
    return false;
  }

  DWORD total_clusters = fs_ptr->n_fatent - 2;
  DWORD sec_per_cluster = fs_ptr->csize;
  if (total_clusters == 0) {
    if (need_unmount)
      f_mount(NULL, path, 0);
    return false;
  }

  uint64_t total_kb64 =
      ((uint64_t)total_clusters * (uint64_t)sec_per_cluster) / 2U;
  if (total_kb64 > 0xFFFFFFFFULL)
    total_kb64 = 0xFFFFFFFFULL;
  *total_kb = (uint32_t)total_kb64;

  DWORD used_clusters = total_clusters - free_clusters;
  *used_pct = (int)(((uint64_t)used_clusters * 100ULL + total_clusters / 2U) /
                    (uint64_t)total_clusters);
  if (*used_pct > 16)
    *used_pct = 100;
  if (*used_pct < 0)
    *used_pct = 0;

  if (need_unmount)
    f_mount(NULL, path, 0);
  return true;
}

// ============ Refresh ============

static void refresh(void) {
  builtin_used_pct = 31; // flash
  builtin_cap_kb = 1024; // 1MB

  /* If SD card is hard-disabled, skip probing entirely â€?   * avoids triggering SysHandle_Exception. */
  if (TSDIO_IsHardDisabled()) {
    sd_present = false;
    sd_cap_kb = 0;
    sd_used_pct = 0;
    return;
  }

  SDCard_Status_t st = boardSDIO.getStatus();
  sd_present = (st == SD_CARD_OK);
  if (st == SD_CARD_ERROR) {
    SysHandle_Exception(SYS_ERR_SD_DISK_ERR);
  }
  if (sd_present) {
    SDCard_Info_t info = boardSDIO.getInfo();
    sd_cap_kb = (uint32_t)info.capacity_mb * 1024;
    int pct;
    uint32_t total;
    if (get_sd_fs_info(&pct, &total)) {
      if (total > 0)
        sd_cap_kb = total;
      sd_used_pct = pct;
    } else {
      sd_used_pct = 0;
    }
  } else {
    sd_cap_kb = 0;
    sd_used_pct = 0;
  }
}

// ============ Draw ============

static void draw_storage(int sel) {
  LCD_FLUSH({
    draw_frame_title("SD");
    PD_SetFont(FONT_ASCII_16);

    draw_card(0, sel, 33, "00 Return");
    draw_card(1, sel, 58, "01 Refresh");

    // Built-In Storage
    int y = 94;
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, y, "Built-In Storage");
    PD_SetColor(TOS_ACCENT);
    const char *sz = fmt_size(builtin_cap_kb);
    uint16_t sw = PD_GetStringWidth(sz);
    PD_DrawString(220 - sw, y, sz);
    draw_progress(14, y + 22, 212, 22, builtin_used_pct, "Used");

    // External Storage (only when card present)
    if (sd_present) {
      y = 152;
      PD_SetColor(TOS_TEXT);
      PD_DrawString(26, y, "External Storage");
      PD_SetColor(TOS_ACCENT);
      sz = fmt_size(sd_cap_kb);
      sw = PD_GetStringWidth(sz);
      PD_DrawString(220 - sw, y, sz);
      draw_progress(14, y + 22, 212, 22, sd_used_pct, "Used");
    }

    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

// ============ Main ============

void storage_activity_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);

  // Show loading
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    PD_DrawFrame();
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, "SD");
    PD_SetColor(TOS_TEXT);
    PD_DrawString(26, 33, "Refreshing...");
  });
  JPDelay(500);

  refresh();

  int sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % 2;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + 2) % 2;
      JPDelay(45);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      if (sel == 0)
        return;
      if (sel == 1) {
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        LCD_FLUSH({
          PD_Init();
          PD_FillScreen(TOS_BG);
          PD_DrawFrame();
          PD_SetFont(FONT_ASCII_16);
          PD_SetColor(TOS_ACCENT);
          PD_DrawString(22, 5, "SD");
          PD_SetColor(TOS_TEXT);
          PD_DrawString(26, 33, "Refreshing...");
        });
        JPDelay(500);
        refresh();
        lu = 0;
      }
    }
    le = ce;
    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      draw_storage(sel);
    }
    JPDelay(1);
  }
}
