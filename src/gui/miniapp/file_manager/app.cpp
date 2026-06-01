/**
 * @file    app.c
 * @brief   File Manager — SD card browser
 */

#include "include/app.h"
#include "components/include/alert.hpp"
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
#include <strings.h>

extern KeyManager keyManager;
extern LCD boardLCD;

#define FM_MAX_ITEMS 32
#define FM_NAME_LEN 64
#define FM_PATH_LEN 192
#define FM_STACK_DEPTH 8
#define FM_DISPLAY_LIMIT 15

static FATFS fm_fs;
static bool fm_mounted = false;

static char fm_cur_path[FM_PATH_LEN] = "0:";
static char fm_items[FM_MAX_ITEMS][FM_NAME_LEN];
static int fm_is_dir[FM_MAX_ITEMS];
static int fm_count = 0;

/* When entering a child directory, remember the selected child index in the
 * parent. When going back through "..", restore that selection instead of
 * jumping back to the top of the parent list. */
static int fm_parent_sel_stack[FM_STACK_DEPTH];
static int fm_stack_depth = 0;

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

static void fm_copy_limited(char *out, size_t out_sz, const char *src,
                            size_t max_chars) {
  if (out_sz == 0) {
    return;
  }
  size_t i = 0;
  while (i + 1U < out_sz && i < max_chars && src[i] != '\0') {
    out[i] = src[i];
    i++;
  }
  out[i] = '\0';
}

static void fm_make_display_name(const char *name, bool is_dir, char *out,
                                 size_t out_sz) {
  if (out_sz == 0) {
    return;
  }
  out[0] = '\0';

  if (strcmp(name, "..") == 0) {
    fm_copy_limited(out, out_sz, "..", 2U);
    return;
  }

  const bool append_slash = is_dir;
  const size_t suffix_len = append_slash ? 1U : 0U;
  const size_t name_len = strlen(name);

  if (name_len + suffix_len <= FM_DISPLAY_LIMIT) {
    size_t i = 0;
    while (i + 1U < out_sz && name[i] != '\0') {
      out[i] = name[i];
      i++;
    }
    if (append_slash && i + 1U < out_sz) {
      out[i++] = '/';
    }
    out[i] = '\0';
    return;
  }

  /* Visible ASCII length <= 15. Folder: 11 + "..." + "/" = 15.
   * File  : 12 + "..."       = 15. Avoid snprintf here because gcc is
   * rightfully conservative about truncation warnings. */
  const size_t keep = append_slash ? 11U : 12U;
  size_t i = 0;
  while (i + 1U < out_sz && i < keep && name[i] != '\0') {
    out[i] = name[i];
    i++;
  }
  const char dots[] = "...";
  for (size_t d = 0; d < 3U && i + 1U < out_sz; ++d) {
    out[i++] = dots[d];
  }
  if (append_slash && i + 1U < out_sz) {
    out[i++] = '/';
  }
  out[i] = '\0';
}

static void draw_card(int idx, int sel, int cy, const char *text, bool is_dir) {
  bool s = (idx == sel);
  PD_DrawAngledCard(14, cy, 212, 20, 5, s ? TOS_ACCENT : TOS_CARD_BG);
  PD_SetColor(s ? TOS_TEXT : TOS_TEXT_SEC);

  char buf[FM_DISPLAY_LIMIT + 2];
  fm_make_display_name(text, is_dir, buf, sizeof(buf));
  PD_DrawString(26, cy + 2, buf);
}

static uint32_t fm_error_code_from_fresult(FRESULT res, uint32_t fallback) {
  switch (res) {
  case FR_OK:
    return SYS_ERR_NONE;
  case FR_NOT_READY:
    return SYS_ERR_SD_NOT_READY;
  case FR_TIMEOUT:
    return SYS_ERR_SD_TIMEOUT;
  case FR_DISK_ERR:
    return SYS_ERR_SD_DISK_ERR;
  case FR_INT_ERR:
    return SYS_ERR_SD_LOST;
  case FR_NO_FILESYSTEM:
    return SYS_ERR_SD_NO_FILESYSTEM;
  default:
    return fallback;
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
    PD_DrawString(22, 33, line1 ? line1 : "Working...");
    if (line2) {
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(TOS_TEXT_SEC);
      PD_DrawString(22, 53, line2);
    }
    PD_DrawFooterCenter("", NULL, "");
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

static bool fm_join_path(const char *base, const char *name, char *out,
                         size_t out_sz) {
  if (out_sz == 0) {
    return false;
  }
  out[0] = '\0';

  const char *sep = (strcmp(base, "0:") == 0) ? "/" : "/";
  size_t need = strlen(base) + strlen(sep) + strlen(name) + 1U;
  if (need > out_sz) {
    return false;
  }

  strcpy(out, base);
  strcat(out, sep);
  strcat(out, name);
  return true;
}

static void fm_build_full_path(const char *name, char *out, size_t out_sz) {
  if (!fm_join_path(fm_cur_path, name, out, out_sz)) {
    fm_copy_limited(out, out_sz, "Path too long", strlen("Path too long"));
  }
}

static void fm_wrap_for_alert(const char *path, char *out, size_t out_sz) {
  if (out_sz == 0) {
    return;
  }

  const size_t width = 22U; /* alert.cpp truncates each line at 22 chars */
  size_t op = 0;
  size_t col = 0;

  const char *prefix = "Path:\n";
  for (const char *p = prefix; *p && op + 1 < out_sz; ++p) {
    out[op++] = *p;
  }

  col = 0;
  for (const char *p = path; *p && op + 1 < out_sz; ++p) {
    if (col >= width) {
      out[op++] = '\n';
      col = 0;
      if (op + 1 >= out_sz) {
        break;
      }
    }
    out[op++] = *p;
    col++;
  }
  out[op] = '\0';
}

static void fm_show_file_path(const char *name) {
  char full[FM_PATH_LEN + FM_NAME_LEN];
  char msg[FM_PATH_LEN + FM_NAME_LEN + 16];

  fm_build_full_path(name, full, sizeof(full));
  fm_wrap_for_alert(full, msg, sizeof(msg));

  LOG_I("FILE", "selected file: %s", full);
  fm_wait_keys_released(600);
  alert_show("FILE", msg);
  fm_wait_keys_released(600);
}

static void fm_push_parent_selection(int sel) {
  if (fm_stack_depth < FM_STACK_DEPTH) {
    fm_parent_sel_stack[fm_stack_depth++] = sel;
  } else {
    /* Keep the newest navigation history if the user enters very deeply. */
    for (int i = 1; i < FM_STACK_DEPTH; ++i) {
      fm_parent_sel_stack[i - 1] = fm_parent_sel_stack[i];
    }
    fm_parent_sel_stack[FM_STACK_DEPTH - 1] = sel;
  }
}

static int fm_pop_parent_selection(void) {
  if (fm_stack_depth <= 0) {
    return 0;
  }
  return fm_parent_sel_stack[--fm_stack_depth];
}

static void fm_trim_to_parent(void) {
  if (strcmp(fm_cur_path, "0:") == 0) {
    return;
  }

  char *p = strrchr(fm_cur_path, '/');
  if (p == NULL || p <= fm_cur_path + 1) {
    strcpy(fm_cur_path, "0:");
  } else {
    *p = '\0';
  }
}

static void fm_load_dir(void) {
  fm_count = 0;
  if (!fm_mounted)
    fm_mount();
  if (!fm_mounted)
    return;

  /* Always show .. for navigation/exit. Keep it at index 0. */
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
    if (fm_count >= FM_MAX_ITEMS)
      break;

    /* With FatFs LFN enabled, fno.fname preserves long names and case.
     * Without LFN it falls back to 8.3 short names, which are often uppercase.
     */
    int idx = fm_count++;
    strncpy(fm_items[idx], fno.fname, FM_NAME_LEN - 1);
    fm_items[idx][FM_NAME_LEN - 1] = '\0';
    fm_is_dir[idx] = (fno.fattrib & AM_DIR) ? 1 : 0;
  }
  f_closedir(&dir);

  /* Sort: directories first, then case-insensitive alphabetical order.
   * Start at 1 so ".." is never sorted into the list. */
  for (int i = 1; i < fm_count - 1; i++) {
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

static bool fm_enter(int idx, int *sel_io) {
  if (idx < 0 || idx >= fm_count) {
    return false;
  }

  if (!fm_is_dir[idx]) {
    fm_show_file_path(fm_items[idx]);
    return false;
  }

  if (strcmp(fm_items[idx], "..") == 0) {
    if (strcmp(fm_cur_path, "0:") == 0) {
      fm_unmount();
      return true; /* signal exit */
    }

    fm_trim_to_parent();
    fm_load_dir();

    int restored = fm_pop_parent_selection();
    if (restored < 0) {
      restored = 0;
    }
    if (restored >= fm_count) {
      restored = fm_count > 0 ? fm_count - 1 : 0;
    }
    if (sel_io != NULL) {
      *sel_io = restored;
    }
    return false;
  }

  char next_path[FM_PATH_LEN];
  if (!fm_join_path(fm_cur_path, fm_items[idx], next_path, sizeof(next_path))) {
    fm_wait_keys_released(600);
    alert_show("FILE", "Path too long");
    return false;
  }

  fm_push_parent_selection(idx);
  strncpy(fm_cur_path, next_path, sizeof(fm_cur_path) - 1);
  fm_cur_path[sizeof(fm_cur_path) - 1] = '\0';
  fm_load_dir();

  if (sel_io != NULL) {
    *sel_io = 0;
  }
  return false;
}

void file_manager_run(void) {
  boardLCD.fillScreen(LCD_COLOR_BLACK);
  if (!fm_prepare_storage()) {
    return;
  }

  strcpy(fm_cur_path, "0:");
  fm_stack_depth = 0;
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
      bool exit_requested = fm_enter(sel, &sel);
      if (exit_requested || !fm_mounted)
        return; /* exit via .. at root */
      n = fm_count;
      if (sel < 0) {
        sel = 0;
      }
      if (sel >= n) {
        sel = n > 0 ? n - 1 : 0;
      }
      lu = 0;
      HAL_Delay(120);
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
        if (start < 0)
          start = 0;

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
