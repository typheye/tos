/**
 * @file    app.c
 * @brief   File Manager — SD card browser
 */

#include "include/app.h"
#include "components/include/confirm.hpp"
#include "core/include/syshandle.h"
#include "core/include/systime.h"
#include "fatfs.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/sfhd.h"
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
  else {
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  }
  PD_DrawString(26, cy + 2, buf);
}

static uint32_t fm_error_code_from_fresult(FRESULT res, uint32_t fallback) {
  switch (res) {
  case FR_OK: return SYS_ERR_NONE;
  case FR_NOT_READY: return SYS_ERR_SD_NOT_READY;
  case FR_TIMEOUT: return SYS_ERR_SD_TIMEOUT;
  case FR_DISK_ERR: return SYS_ERR_SD_DISK_ERR;
  case FR_INT_ERR: return SYS_ERR_SD_LOST;
  case FR_NO_FILESYSTEM: return SYS_ERR_SD_NO_FILESYSTEM;
  default: return fallback;
  }
}

static void fm_fatal_if_storage_error(FRESULT res, uint32_t fallback) {
  uint32_t code = fm_error_code_from_fresult(res, fallback);
  if (code == SYS_ERR_SD_NOT_READY || code == SYS_ERR_SD_TIMEOUT ||
      code == SYS_ERR_SD_DISK_ERR || code == SYS_ERR_SD_LOST) {
    SysHandle_Exception(code);
  }
}

static FRESULT fm_mount_result(void) {
  if (!fm_mounted) {
    FRESULT res = f_mount(&fm_fs, "0:", 1);
    if (res == FR_OK) {
      fm_mounted = true;
    }
    return res;
  }
  return FR_OK;
}

static void fm_mount(void) {
  FRESULT res = fm_mount_result();
  fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
}

static void fm_unmount(void) {
  if (fm_mounted) {
    f_mount(NULL, "0:", 0);
    fm_mounted = false;
  }
}

static void fm_draw_formatting(const char *line1, const char *line2) {
  LCD_FLUSH({
    draw_frame_title("FILE");
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_TEXT);
    PD_DrawString(22, 58, line1 ? line1 : "Working...");
    if (line2) {
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(TOS_TEXT_SEC);
      PD_DrawString(22, 88, line2);
    }
    PD_DrawFooterCenter("WAIT", NULL, "");
  });
}

static void fm_format_progress(const char *step, FRESULT res, void *user) {
  (void)user;

  char line1[40];
  char line2[48];
  snprintf(line1, sizeof(line1), "%s", step ? step : "Working");
  snprintf(line2, sizeof(line2), "%s (%d)", SFHD_FResultName(res), (int)res);
  fm_draw_formatting(line1, line2);

  /* Keep progress visible but do not slow the full format too much. */
  HAL_Delay(120);
}

static FRESULT fm_format_and_init_sd(void) {
  SFHD_SD_FormatOptions_t opt;
  opt.progress = fm_format_progress;
  opt.user = NULL;

  fm_draw_formatting("Formatting SD card", "Please do not power off");

  FRESULT res = SFHD_SD_FormatAndInit(&opt);
  if (res != FR_OK) {
    char line1[40];
    char line2[48];
    snprintf(line1, sizeof(line1), "Format failed");
    snprintf(line2, sizeof(line2), "%s (%d)", SFHD_FResultName(res), (int)res);
    fm_draw_formatting(line1, line2);
    HAL_Delay(1200);
    return res;
  }

  fm_mounted = true;
  fm_draw_formatting("Format complete", "Opening file manager");
  HAL_Delay(600);
  return FR_OK;
}

static bool fm_has_init_marker(void) {
  FILINFO info;
  FRESULT res = f_stat("0:/init", &info);
  if (res == FR_OK) {
    return true;
  }
  if (res == FR_NO_FILE || res == FR_NO_PATH || res == FR_NO_FILESYSTEM) {
    return false;
  }

  fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
  return false;
}


static void fm_wait_keys_released(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() != KEY_PRESSED &&
        keyManager.collision_D0.getState() != KEY_PRESSED &&
        keyManager.btn_enter.getState() != KEY_PRESSED) {
      return;
    }
    HAL_Delay(5);
  }
}

static bool fm_prepare_storage(void) {
  SFHD_SD_DebugProbe("file-manager-entry");

  FRESULT res = fm_mount_result();
  LOG_I("FILE", "mount result: %s(%d)", SFHD_FResultName(res), (int)res);
  if (res == FR_NO_FILESYSTEM) {
    fm_mounted = false;
  } else if (res != FR_OK) {
    fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
    return false;
  }

  if (fm_mounted && fm_has_init_marker()) {
    return true;
  }

  fm_wait_keys_released(800);
  bool do_format = confirm_show("FILE", "Format SD card?");
  if (!do_format) {
    fm_unmount();
    return false;
  }

  res = fm_format_and_init_sd();
  LOG_I("FILE", "format result: %s(%d)", SFHD_FResultName(res), (int)res);
  if (res == FR_OK && fm_has_init_marker()) {
    return true;
  }

  fm_fatal_if_storage_error(res, SYS_ERR_SD_FORMAT_FAILED);
  SysHandle_Exception(SFHD_FResultToSysError(res));
  return false;
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
  FRESULT res = f_opendir(&dir, fm_cur_path);
  if (res != FR_OK) {
    fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
    return;
  }

  while (1) {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK) {
      f_closedir(&dir);
      fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
      return;
    }
    if (!fno.fname[0]) {
      break;
    }
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
  if (!fm_prepare_storage()) {
    return;
  }

  strcpy(fm_cur_path, "0:");
  fm_load_dir();

  int n = fm_count, sel = 0;
  uint32_t lu = 0;

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (n <= 0) {
      fm_load_dir();
      n = fm_count;
      sel = 0;
    }

    if (n > 0 && keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % n;
      HAL_Delay(100);
    }
    if (n > 0 && keyManager.collision_D0.getState() == KEY_PRESSED) {
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
      LCD_FLUSH({
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
      });
    }
    HAL_Delay(1);
  }
}
