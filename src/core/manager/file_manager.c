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

#include "file_manager.h"

#include "syshandle.h"
/* C-compatible SD hard-disabled check (defined in hardware/tsdio.cpp) */
extern bool TSDIO_IsHardDisabled(void);
#include "syslog.h"
#include <stdio.h>
#include <string.h>

static bool g_fmcore_mounted = false;

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
  FRESULT res = FR_OK;

  /* If SD card is hard-disabled, return immediately without touching the
   * hardware.  This prevents FatFs from calling into a broken diskio layer
   * and triggering a SysHandle_Exception. */
  if (TSDIO_IsHardDisabled()) {
    LOG_W("FMCR", "mount blocked: SD card is hard-disabled");
    return FR_NOT_READY;
  }

  if (!g_fmcore_mounted) {
    res = f_mount(fs, "0:", 1);
    if (res == FR_OK) g_fmcore_mounted = true;
    LOG_I("FMCR", "mount => %s(%d)", FMCore_FResultName(res), (int)res);
  }
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
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  }
  if (res == FR_OK) res = f_sync(&fp);
  FRESULT close_res = f_close(&fp);
  if (res == FR_OK) res = close_res;
  LOG_I("FMCR", "create %s => %s(%d)", path, FMCore_FResultName(res), (int)res);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
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

FRESULT FMCore_InitLayout(bool fatal_on_storage_error) {
  static const char *dirs[] = {
      "0:/data", "0:/oem", "0:/dev", "0:/storage", "0:/system"};
  FRESULT res = FR_OK;
  for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
    res = FMCore_CreateDir(dirs[i], fatal_on_storage_error);
    if (res != FR_OK) return res;
  }
  res = FMCore_CreateFile("0:/init", "TOS filesystem ready\r\n", 22U,
                          fatal_on_storage_error);
  return res;
}
