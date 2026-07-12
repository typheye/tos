/**
 ******************************************************************************
 * @file    app.cpp
 * @author  Typheye
 * @brief   App implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/app.h"
#include "dram.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern KeyManager keyManager;
extern LCD boardLCD;

#define FM_MAX_ITEMS 32
#define FM_NAME_LEN 64
#define FM_PATH_LEN 192
#define FM_STACK_DEPTH 8
#define FM_DISPLAY_LIMIT 15

static FATFS fm_fs;
static bool fm_mounted = false;

static char *fm_cur_path = nullptr;
static char (*fm_items)[FM_NAME_LEN] = nullptr;
static int *fm_is_dir = nullptr;
static int fm_count = 0;

/* When entering a child directory, remember the selected child index in the
 * parent. When going back through "..", restore that selection instead of
 * jumping back to the top of the parent list. */
static int *fm_parent_sel_stack = nullptr;
static int fm_stack_depth = 0;

static bool fm_alloc_context(void) {
  if (fm_cur_path && fm_items && fm_is_dir && fm_parent_sel_stack)
    return true;

  fm_cur_path = (char *)SysDram_Alloc(FM_PATH_LEN);
  fm_items = (char (*)[FM_NAME_LEN])SysDram_Alloc(FM_MAX_ITEMS * FM_NAME_LEN);
  fm_is_dir = (int *)SysDram_Alloc(FM_MAX_ITEMS * sizeof(int));
  fm_parent_sel_stack = (int *)SysDram_Alloc(FM_STACK_DEPTH * sizeof(int));

  if (fm_cur_path && fm_items && fm_is_dir && fm_parent_sel_stack) {
    strcpy(fm_cur_path, "0:");
    memset(fm_items, 0, FM_MAX_ITEMS * FM_NAME_LEN);
    memset(fm_is_dir, 0, FM_MAX_ITEMS * sizeof(int));
    memset(fm_parent_sel_stack, 0, FM_STACK_DEPTH * sizeof(int));
    fm_count = 0;
    fm_stack_depth = 0;
    return true;
  }

  SysDram_Free(fm_cur_path);
  SysDram_Free(fm_items);
  SysDram_Free(fm_is_dir);
  SysDram_Free(fm_parent_sel_stack);
  fm_cur_path = nullptr;
  fm_items = nullptr;
  fm_is_dir = nullptr;
  fm_parent_sel_stack = nullptr;
  fm_count = 0;
  fm_stack_depth = 0;
  return false;
}

static void fm_free_context(void) {
  SysDram_Free(fm_parent_sel_stack);
  SysDram_Free(fm_is_dir);
  SysDram_Free(fm_items);
  SysDram_Free(fm_cur_path);
  fm_parent_sel_stack = nullptr;
  fm_is_dir = nullptr;
  fm_items = nullptr;
  fm_cur_path = nullptr;
  fm_count = 0;
  fm_stack_depth = 0;
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

static void fm_fatal_if_storage_error(FRESULT res, uint32_t fallback) {
  SysHandle_FatalFResult(res, fallback);
}

static FRESULT fm_mount_result(void) {
  if (!fm_mounted) {
    FRESULT res = FMCore_Mount(&fm_fs, false);
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
    FMCore_Unmount();
    fm_mounted = false;
  }
}

static void fm_wait_keys_released(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms) {
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();
    if (keyManager._collisionA8.getState() != KEY_PRESSED &&
        keyManager._collisionD0.getState() != KEY_PRESSED &&
        keyManager._btnEnter.getState() != KEY_PRESSED) {
      return;
    }
    JPDelay(5);
  }
}

static bool fm_prepare_storage(void) {
  FRESULT res = fm_mount_result();
  LOG_I("FILE", "mount result: %s(%d)", SFHD_FResultName(res), (int)res);
  if (res != FR_OK) {
    fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
    return false;
  }
  return true;
}

static void fm_build_full_path(const char *name, char *out, size_t out_sz) {
  if (!FMCore_JoinPath(fm_cur_path, name, out, out_sz)) {
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
  if (strcmp(fm_cur_path, "/") == 0) {
    return;
  }

  char *p = strrchr(fm_cur_path, '/');
  if (p == NULL || p == fm_cur_path) {
    strcpy(fm_cur_path, "/");
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

  FMCore_Entry entries[FM_MAX_ITEMS - 1];
  uint16_t n = 0;
  FRESULT res =
      FMCore_ListDir(fm_cur_path, entries, FM_MAX_ITEMS - 1, &n, true);
  if (res != FR_OK) {
    fm_fatal_if_storage_error(res, SYS_ERR_SD_BROWSER_FAILED);
    return;
  }

  for (uint16_t i = 0; i < n && fm_count < FM_MAX_ITEMS; ++i) {
    int idx = fm_count++;
    strncpy(fm_items[idx], entries[i].name, FM_NAME_LEN - 1);
    fm_items[idx][FM_NAME_LEN - 1] = '\0';
    fm_is_dir[idx] = entries[i].is_dir ? 1 : 0;
  }

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
        memcpy(tn, fm_items[i], FM_NAME_LEN);
        td = fm_is_dir[i];
        memcpy(fm_items[i], fm_items[j], FM_NAME_LEN);
        fm_is_dir[i] = fm_is_dir[j];
        memcpy(fm_items[j], tn, FM_NAME_LEN);
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
    if (strcmp(fm_cur_path, "/") == 0) {
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
  if (!FMCore_JoinPath(fm_cur_path, fm_items[idx], next_path,
                       sizeof(next_path))) {
    fm_wait_keys_released(600);
    alert_show("FILE", "Path too long");
    return false;
  }

  fm_push_parent_selection(idx);
  strncpy(fm_cur_path, next_path, FM_PATH_LEN - 1);
  fm_cur_path[FM_PATH_LEN - 1] = '\0';
  fm_load_dir();

  if (sel_io != NULL) {
    *sel_io = 0;
  }
  return false;
}

void file_manager_run(void) {
  if (!fm_alloc_context()) {
    alert_show("FILE", "Memory failed");
    return;
  }

  if (!fm_prepare_storage()) {
    fm_free_context();
    return;
  }

  strcpy(fm_cur_path, "/");
  fm_stack_depth = 0;
  fm_load_dir();

  int n = fm_count, sel = 0;
  uint32_t lu = 0;
  uint32_t last_probe = 0;

  while (1) {
    TosApi_Tick();
    keyManager._collisionA8.tick();
    keyManager._collisionD0.tick();
    keyManager._btnEnter.tick();

    if (n <= 0) {
      fm_load_dir();
      n = fm_count;
      sel = 0;
    }

    if (n > 0 && keyManager._collisionA8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % n;
      JPDelay(45);
    }
    if (n > 0 && keyManager._collisionD0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + n) % n;
      JPDelay(45);
    }

    if ((HAL_GetTick() - last_probe) > 1200U && fm_mounted &&
        FMCore_IsInitialized()) {
      last_probe = HAL_GetTick();
      FILINFO ping;
      FRESULT pr = FMCore_Stat("/init", &ping, false);
      if (SysHandle_IsStorageFatal(pr)) {
        SysHandle_FatalFResult(pr, SYS_ERR_SD_LOST);
      }
    }
    if (keyManager._btnEnter.getState() == KEY_PRESSED) {
      if (n == 0) {
        fm_unmount();
        fm_free_context();
        return;
      }
      bool exit_requested = fm_enter(sel, &sel);
      if (exit_requested || !fm_mounted) {
        fm_free_context();
        return; /* exit via .. at root */
      }
      n = fm_count;
      if (sel < 0) {
        sel = 0;
      }
      if (sel >= n) {
        sel = n > 0 ? n - 1 : 0;
      }
      lu = 0;
      JPDelay(45);
    }

    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        UI_DrawFrameTitle("FILE");
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
    TosApi_Tick();
    JPDelay(1);
  }
}
