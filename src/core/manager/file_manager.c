/**
 ******************************************************************************
 * @file    file_manager.c
 * @author  Typheye
 * @brief   File Manager implementation.
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

#include "include/file_manager.h"


/* C-compatible SD hard-disabled check (defined in hardware/tsdio.cpp) */
extern bool TSDIO_IsHardDisabled(void);
extern bool TSDIO_IsInitialized(void);

static bool g_fmcore_mounted = false;
static FATFS g_fmcore_fs;
static char g_boot_log_path[FMCORE_PATH_MAX];
static uint8_t g_boot_log_fail_count = 0;
static bool g_boot_log_ready_seen = false;
static bool g_boot_log_fatal_fault = false;

#define FMCORE_BOOT_LOG_TIMEOUT_MS 3500U

static FRESULT fmcore_mount_impl(FATFS *fs, bool log_result) {
  FRESULT res = FR_OK;

  if (TSDIO_IsHardDisabled() || !TSDIO_IsInitialized()) {
    if (log_result) LOG_W("FMCR", "mount blocked: SD card is not ready");
    return FR_NOT_READY;
  }

  if (!g_fmcore_mounted) {
    res = f_mount(fs ? fs : &g_fmcore_fs, "0:", 1);
    if (res == FR_OK) g_fmcore_mounted = true;
    if (log_result) {
      LOG_I("FMCR", "mount => %s(%d)", FMCore_FResultName(res), (int)res);
    }
  }
  return res;
}

static FRESULT fmcore_mount_quiet(void) {
  return fmcore_mount_impl(&g_fmcore_fs, false);
}

static FRESULT fmcore_ensure_dir_quiet(const char *path) {
  FRESULT res = f_mkdir(path);
  if (res == FR_EXIST) res = FR_OK;
  return res;
}

static bool fmcore_has_init_marker_quiet(void) {
  FILINFO info;
  if (fmcore_mount_quiet() != FR_OK) return false;
  if (f_stat("0:/init", &info) != FR_OK) return false;
  return (info.fattrib & AM_DIR) == 0U;
}

const char *FMCore_FResultName(FRESULT res) {
  switch (res) {
  case FR_OK: return "FR_OK";
  case FR_DISK_ERR: return "FR_DISK_ERR";
  case FR_INT_ERR: return "FR_INT_ERR";
  case FR_NOT_READY: return "FR_NOT_READY";
  case FR_NO_FILE: return "FR_NO_FILE";
  case FR_NO_PATH: return "FR_NO_PATH";
  case FR_INVALID_NAME: return "FR_INVALID_NAME";
  case FR_DENIED: return "FR_DENIED";
  case FR_EXIST: return "FR_EXIST";
  case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
  case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
  case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
  case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
  case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
  case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
  case FR_TIMEOUT: return "FR_TIMEOUT";
  case FR_LOCKED: return "FR_LOCKED";
  case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
  case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
  case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
  default: return "FR_UNKNOWN";
  }
}

static void fatal_if_needed(FRESULT res, bool fatal_on_storage_error,
                            uint32_t fallback) {
  if (fatal_on_storage_error) {
    SysHandle_FatalFResult(res, fallback);
  }
}

FRESULT FMCore_Mount(FATFS *fs, bool fatal_on_storage_error) {
  FRESULT res = fmcore_mount_impl(fs, true);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
  return res;
}

void FMCore_Unmount(void) {
  if (g_fmcore_mounted) {
    (void)f_mount(NULL, "0:", 0);
    g_fmcore_mounted = false;
    LOG_I("FMCR", "unmount");
  }
}

FRESULT FMCore_Stat(const char *path, FILINFO *info, bool fatal_on_storage_error) {
  FRESULT res = f_stat(path, info);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

bool FMCore_JoinPath(const char *base, const char *name, char *out, size_t out_sz) {
  if (!base || !name || !out || out_sz == 0U) return false;
  const char *sep = "/";
  size_t need = strlen(base) + strlen(sep) + strlen(name) + 1U;
  if (need > out_sz) return false;
  strcpy(out, base);
  strcat(out, sep);
  strcat(out, name);
  return true;
}

FRESULT FMCore_ListDir(const char *path, FMCore_Entry *entries, uint16_t max_entries,
                       uint16_t *out_count, bool fatal_on_storage_error) {
  DIR dir;
  FILINFO fno;
  FRESULT res;
  uint16_t count = 0;

  if (out_count) *out_count = 0;
  if (!path || !entries || max_entries == 0U) return FR_INVALID_PARAMETER;

  res = f_opendir(&dir, path);
  if (res != FR_OK) {
    LOG_E("FMCR", "opendir %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
    return res;
  }

  while (count < max_entries) {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK) {
      (void)f_closedir(&dir);
      LOG_E("FMCR", "readdir %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
      fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
      return res;
    }
    if (fno.fname[0] == '\0') break;

    strncpy(entries[count].name, fno.fname, FMCORE_NAME_MAX - 1U);
    entries[count].name[FMCORE_NAME_MAX - 1U] = '\0';
    entries[count].is_dir = (fno.fattrib & AM_DIR) ? 1U : 0U;
    entries[count].size = fno.fsize;
    entries[count].attr = fno.fattrib;
    ++count;
  }
  (void)f_closedir(&dir);
  if (out_count) *out_count = count;
  return FR_OK;
}

FRESULT FMCore_CreateDir(const char *path, bool fatal_on_storage_error) {
  FRESULT res = f_mkdir(path);
  if (res == FR_EXIST) res = FR_OK;
  LOG_I("FMCR", "mkdir %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_CreateFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error) {
  FIL fp;
  UINT bw = 0;
  FRESULT res = f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
    return res;
  }
  if (len > 0U && data != NULL) {
    SysWatchdog_Tick();
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  }
  if (res == FR_OK) {
    SysWatchdog_Tick();
    res = f_sync(&fp);
  }
  SysWatchdog_Tick();
  FRESULT close_res = f_close(&fp);
  if (res == FR_OK) res = close_res;
  LOG_I("FMCR", "create %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_WriteFile(const char *path, const void *data, uint32_t len,
                         bool fatal_on_storage_error) {
  return FMCore_CreateFile(path, data, len, fatal_on_storage_error);
}

FRESULT FMCore_ReadFile(const char *path, void *buf, uint32_t max_len,
                        uint32_t *out_len, bool fatal_on_storage_error) {
  FIL fp;
  UINT br = 0;
  FRESULT res;

  if (out_len) *out_len = 0U;
  if (!path || !buf || max_len == 0U) return FR_INVALID_PARAMETER;

  res = f_open(&fp, path, FA_READ);
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
    return res;
  }

  res = f_read(&fp, buf, (UINT)max_len, &br);
  if (out_len) *out_len = (uint32_t)br;
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_AppendFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error) {
  FIL fp;
  UINT bw = 0;
  FRESULT res;

  if (!path || (len > 0U && !data)) return FR_INVALID_PARAMETER;
  res = fmcore_mount_quiet();
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
    return res;
  }

  res = f_open(&fp, path, FA_OPEN_APPEND | FA_WRITE);
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
    return res;
  }

  if (len > 0U) {
    SysWatchdog_Tick();
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  }
  if (res == FR_OK) {
    SysWatchdog_Tick();
    res = f_sync(&fp);
  }
  SysWatchdog_Tick();
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

static bool fmcore_op_timed_out(uint32_t start, uint32_t timeout_ms) {
  return timeout_ms > 0U && (uint32_t)(HAL_GetTick() - start) > timeout_ms;
}

static FRESULT fmcore_append_file_timed(const char *path, const void *data,
                                        uint32_t len, uint32_t timeout_ms) {
  FIL fp;
  UINT bw = 0;
  FRESULT res;
  uint32_t start = HAL_GetTick();

  if (!path || (len > 0U && !data)) return FR_INVALID_PARAMETER;

  SysWatchdog_FeedNow();
  res = fmcore_mount_quiet();
  if (res != FR_OK) return res;
  if (fmcore_op_timed_out(start, timeout_ms)) return FR_TIMEOUT;

  SysWatchdog_FeedNow();
  res = f_open(&fp, path, FA_OPEN_APPEND | FA_WRITE);
  if (res != FR_OK) return res;
  if (fmcore_op_timed_out(start, timeout_ms)) {
    (void)f_close(&fp);
    return FR_TIMEOUT;
  }

  if (len > 0U) {
    SysWatchdog_FeedNow();
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
    if (res == FR_OK && fmcore_op_timed_out(start, timeout_ms)) res = FR_TIMEOUT;
  }

  if (res == FR_OK) {
    SysWatchdog_FeedNow();
    res = f_sync(&fp);
    if (res == FR_OK && fmcore_op_timed_out(start, timeout_ms)) res = FR_TIMEOUT;
  }

  SysWatchdog_FeedNow();
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
  if (res == FR_OK && fmcore_op_timed_out(start, timeout_ms)) res = FR_TIMEOUT;
  SysWatchdog_FeedNow();
  return res;
}

static FRESULT delete_dir_recursive(const char *path, bool fatal_on_storage_error) {
  FMCore_Entry entries[8];
  uint16_t count = 0;
  FRESULT res = FMCore_ListDir(path, entries, 8, &count, fatal_on_storage_error);
  if (res != FR_OK) return res;

  while (count > 0) {
    for (uint16_t i = 0; i < count; ++i) {
      char child[FMCORE_PATH_MAX];
      if (!FMCore_JoinPath(path, entries[i].name, child, sizeof(child))) {
        return FR_INVALID_NAME;
      }
      if (entries[i].is_dir) res = delete_dir_recursive(child, fatal_on_storage_error);
      else res = f_unlink(child);
      if (res != FR_OK) {
        fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
        return res;
      }
    }
    res = FMCore_ListDir(path, entries, 8, &count, fatal_on_storage_error);
    if (res != FR_OK) return res;
  }
  return f_unlink(path);
}

FRESULT FMCore_Delete(const char *path, bool recursive, bool fatal_on_storage_error) {
  FILINFO info;
  FRESULT res = f_stat(path, &info);
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
    return res;
  }
  if ((info.fattrib & AM_DIR) && recursive) res = delete_dir_recursive(path, fatal_on_storage_error);
  else res = f_unlink(path);
  LOG_I("FMCR", "delete %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_CopyFile(const char *src, const char *dst, bool fatal_on_storage_error) {
  FIL in;
  FIL out;
  uint8_t buf[256];
  UINT br = 0;
  UINT bw = 0;
  FRESULT res = f_open(&in, src, FA_READ);
  if (res != FR_OK) goto done_no_in;
  res = f_open(&out, dst, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) goto done_in;
  do {
    res = f_read(&in, buf, sizeof(buf), &br);
    if (res != FR_OK || br == 0U) break;
    res = f_write(&out, buf, br, &bw);
    if (res != FR_OK || bw != br) {
      if (res == FR_OK) res = FR_DISK_ERR;
      break;
    }
  } while (br == sizeof(buf));
  if (res == FR_OK) res = f_sync(&out);
  {
    FRESULT close_out = f_close(&out);
    if (res == FR_OK) res = close_out;
  }

done_in:
  {
    FRESULT close_in = f_close(&in);
    if (res == FR_OK) res = close_in;
  }

done_no_in:
  LOG_I("FMCR", "copy %s -> %s => %s(%d)", src, dst, FMCore_FResultName(res), (int)res);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

bool FMCore_IsInitialized(void) {
  return !TSDIO_IsHardDisabled() && TSDIO_IsInitialized() &&
         fmcore_has_init_marker_quiet();
}

FRESULT FMCore_NextIndexedPath(const char *dir, const char *ext,
                               char *out, size_t out_sz) {
  FILINFO info;
  if (!dir || !ext || !out || out_sz == 0U) return FR_INVALID_PARAMETER;

  for (uint32_t idx = 1U; idx <= 99999999UL; ++idx) {
    int n = snprintf(out, out_sz, "%s/%08lu.%s", dir,
                     (unsigned long)idx, ext);
    if (n <= 0 || (size_t)n >= out_sz) return FR_INVALID_NAME;
    FRESULT res = f_stat(out, &info);
    if (res == FR_NO_FILE || res == FR_NO_PATH) return FR_OK;
    if (res != FR_OK) return res;
  }
  return FR_DENIED;
}

static FRESULT fmcore_prepare_system_dir(const char *subdir) {
  FRESULT res;

  if (!FMCore_IsInitialized()) return FR_NOT_READY;
  res = fmcore_ensure_dir_quiet("0:/system");
  if (res != FR_OK) return res;
  return fmcore_ensure_dir_quiet(subdir);
}

FRESULT FMCore_AppendBootLog(const char *line, uint32_t len) {
  FRESULT res;

  if (!line || len == 0U) return FR_INVALID_PARAMETER;
  if (g_boot_log_fatal_fault) return FR_NOT_READY;

  if (!g_boot_log_ready_seen) {
    res = fmcore_prepare_system_dir("0:/system/log");
    if (res != FR_OK) {
      if (res != FR_NOT_READY) {
        g_boot_log_fatal_fault = true;
        if (g_boot_log_fail_count < 255U) g_boot_log_fail_count++;
      }
      return res;
    }
    g_boot_log_ready_seen = true;
  }

  if (g_boot_log_path[0] == '\0') {
    res = FMCore_NextIndexedPath("0:/system/log", "log",
                                 g_boot_log_path, sizeof(g_boot_log_path));
    if (res != FR_OK) {
      g_boot_log_fatal_fault = true;
      if (g_boot_log_fail_count < 255U) g_boot_log_fail_count++;
      return res;
    }
  }

  res = fmcore_append_file_timed(g_boot_log_path, line, len,
                                 FMCORE_BOOT_LOG_TIMEOUT_MS);
  if (res != FR_OK) {
    g_boot_log_fatal_fault = true;
    if (g_boot_log_fail_count < 255U) g_boot_log_fail_count++;
  }
  return res;
}

bool FMCore_IsBootLogFaultFatal(void) {
  return g_boot_log_fatal_fault;
}

FRESULT FMCore_WriteSystemDump(uint32_t code, const char *name,
                               const char *extra) {
  char path[FMCORE_PATH_MAX];
  char body[512];
  FRESULT res;
  int n;

  res = fmcore_prepare_system_dir("0:/system/dump");
  if (res != FR_OK) return res;
  res = FMCore_NextIndexedPath("0:/system/dump", "dump", path, sizeof(path));
  if (res != FR_OK) return res;

  n = snprintf(body, sizeof(body),
               "TOS system dump\r\n"
               "code=0x%08lX\r\n"
               "type=%s\r\n"
               "%s%s",
               (unsigned long)code,
               name ? name : "UNKNOWN",
               extra ? extra : "",
               (extra && extra[0]) ? "" : "\r\n");
  if (n <= 0) return FR_INVALID_PARAMETER;
  if (n >= (int)sizeof(body)) n = (int)sizeof(body) - 1;

  return FMCore_CreateFile(path, body, (uint32_t)n, false);
}

FRESULT FMCore_InitLayout(bool fatal_on_storage_error) {
  static const char *dirs[] = {
      "0:/data", "0:/oem", "0:/dev", "0:/storage", "0:/system",
      "0:/system/log", "0:/system/dump"};
  FRESULT res = FR_OK;
  for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
    res = FMCore_CreateDir(dirs[i], fatal_on_storage_error);
    if (res != FR_OK) return res;
  }
  res = FMCore_CreateFile("0:/init", "TOS filesystem ready\r\n", 22U,
                          fatal_on_storage_error);
  return res;
}
