/**
 * @file    app.c
 * @brief   File Manager — SD card browser
 */

#include "include/app.h"
#include "core/include/systime.h"
#include "fatfs.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "syslog.h"
#include <cstdio>
#include <cstring>

extern KeyManager keyManager;
extern LCD boardLCD;

#define FM_MAX_ITEMS 32
#define FM_NAME_LEN 32

static FATFS fm_fs;
static bool fm_mounted = false;

static char fm_cur_path[128] = "0:";
static char fm_items[FM_MAX_ITEMS][FM_NAME_LEN];
static int fm_is_dir[FM_MAX_ITEMS];
static int fm_count = 0;

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

static void draw_card(int idx, int sel, int cy, const char *text, bool is_dir) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);
  /* Append / for directories */
  char buf[36];
  if (is_dir && strcmp(text, "..") != 0)
    snprintf(buf, sizeof(buf), "%s/", text);
  else
    strncpy(buf, text, sizeof(buf) - 1);
  PD_DrawString(26, cy + 2, buf);
}

static void fm_mount(void) {
  if (!fm_mounted) {
    fm_mounted = (f_mount(&fm_fs, "0:", 1) == FR_OK);
  }
}

static void fm_unmount(void) {
  if (fm_mounted) {
    f_mount(NULL, "0:", 0);
    fm_mounted = false;
  }
}

static void fm_load_dir(void) {
  fm_count = 0;
  if (!fm_mounted)
    fm_mount();
  if (!fm_mounted)
    return;

  /* Always show .. for navigation/exit */
  strcpy(fm_items[0], "..");
  fm_is_dir[0] = 1;
  fm_count = 1;

  DIR dir;
  FILINFO fno;
  if (f_opendir(&dir, fm_cur_path) != FR_OK)
    return;
  while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
    if (fm_count >= FM_MAX_ITEMS - 1)
      break;
    int idx = fm_count++;
    strncpy(fm_items[idx], fno.fname, FM_NAME_LEN - 1);
    fm_items[idx][FM_NAME_LEN - 1] = '\0';
    fm_is_dir[idx] = (fno.fattrib & AM_DIR) ? 1 : 0;
  }
  f_closedir(&dir);

  /* Sort: dirs first */
  for (int i = (strcmp(fm_cur_path, "0:") == 0 ? 0 : 1); i < fm_count - 1;
       i++) {
    for (int j = i + 1; j < fm_count; j++) {
      bool swap = false;
      if (fm_is_dir[i] && !fm_is_dir[j])
        continue;
      if (!fm_is_dir[i] && fm_is_dir[j])
        swap = true;
      else if (strcasecmp(fm_items[i], fm_items[j]) > 0)
        swap = true;
      if (swap) {
        char tn[FM_NAME_LEN];
        int td;
        strcpy(tn, fm_items[i]);
        td = fm_is_dir[i];
        strcpy(fm_items[i], fm_items[j]);
        fm_is_dir[i] = fm_is_dir[j];
        strcpy(fm_items[j], tn);
        fm_is_dir[j] = td;
      }
    }
  }
}

static void fm_enter(int idx) {
  if (!fm_is_dir[idx])
    return;
  if (strcmp(fm_items[idx], "..") == 0) {
    if (strcmp(fm_cur_path, "0:") == 0) {
      fm_unmount();
      return; /* signal exit */
    }
    char *p = strrchr(fm_cur_path, '/');
    if (p)
      *p = '\0';
    fm_load_dir();
  } else {
    int len = strlen(fm_cur_path);
    snprintf(fm_cur_path + len, sizeof(fm_cur_path) - len, "/%s",
             fm_items[idx]);
    fm_load_dir();
  }
}

void file_manager_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  fm_mount();
  strcpy(fm_cur_path, "0:");
  fm_load_dir();

  int n = fm_count, sel = 0;
  uint32_t lu = 0;

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
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      if (n == 0) {
        fm_unmount();
        return;
      }
      fm_enter(sel);
      if (!fm_mounted)
        return; /* exit via .. at root */
      n = fm_count;
      sel = 0;
    }

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      draw_frame_title("FILE");
      PD_SetFont(FONT_ASCII_16);
      int vis = n < 7 ? n : 7;
      if (n == 0)
        vis = 1;
      int start = sel - vis / 2;
      if (start < 0)
        start = 0;
      if (start + vis > n)
        start = n - vis;

      if (n == 0) {
        draw_card(0, 0, 33, "   No files", false);
      } else {
        for (int i = 0; i < vis; i++) {
          int idx = start + i;
          if (idx >= n)
            break;
          draw_card(idx, sel, 33 + i * 25, fm_items[idx], fm_is_dir[idx]);
        }
      }
      PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
