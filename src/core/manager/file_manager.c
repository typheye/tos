/**
 ******************************************************************************
 * @file    file_manager.c
 * @author  Typheye
 * @brief   File Manager implementation.
 ******************************************************************************
 */

#include "include/file_manager.h"
#include "hardware/include/flash_diskio.h"
#include "tos_partitions.h"

extern bool TSDIO_IsHardDisabled(void);
extern bool TSDIO_IsInitialized(void);

static bool g_storage_mounted;
static bool g_internal_mounted;
static FATFS g_storage_fs;
static char g_boot_log_path[FMCORE_PATH_MAX];
static bool g_boot_log_ready_seen;

#define FMCORE_BOOT_LOG_TIMEOUT_MS 3500U
#define FMCORE_LOG_DIR  "0:/storage/tos/log"
#define FMCORE_DUMP_DIR "0:/storage/tos/dump"

typedef enum {
  FM_PATH_STORAGE = 0,
  FM_PATH_TMP,
  FM_PATH_DATA,
  FM_PATH_INIT,
  FM_PATH_VIRTUAL_ROOT,
  FM_PATH_RAW
} FMPathKind;

static bool path_prefix(const char *path, const char *prefix) {
  size_t n = strlen(prefix);
  return strncmp(path, prefix, n) == 0 &&
         (path[n] == '\0' || path[n] == '/');
}

static FRESULT translate_path(const char *path, char *out, size_t out_sz,
                              FMPathKind *kind) {
  const char *suffix = "";
  const char *base = NULL;
  if (!path || !out || out_sz == 0U) return FR_INVALID_PARAMETER;

  if (strcmp(path, "/") == 0 || path[0] == '\0') {
    if (kind) *kind = FM_PATH_VIRTUAL_ROOT;
    out[0] = '\0';
    return FR_OK;
  }
  if (path[0] >= '0' && path[0] <= '9' && path[1] == ':') {
    if (strlen(path) + 1U > out_sz) return FR_INVALID_NAME;
    strcpy(out, path);
    if (kind) *kind = FM_PATH_RAW;
    return FR_OK;
  }
  if (path_prefix(path, "/storage")) {
    base = "0:/storage";
    suffix = path + strlen("/storage");
    if (kind) *kind = FM_PATH_STORAGE;
  } else if (path_prefix(path, "/tmp")) {
    base = TMPPath;
    suffix = path + strlen("/tmp");
    if (kind) *kind = FM_PATH_TMP;
  } else if (path_prefix(path, "/data")) {
    base = DataPath;
    suffix = path + strlen("/data");
    if (kind) *kind = FM_PATH_DATA;
  } else if (strcmp(path, "/init") == 0) {
    base = "0:/init";
    if (kind) *kind = FM_PATH_INIT;
  } else {
    return FR_INVALID_NAME;
  }

  while (*suffix == '/') suffix++;
  size_t base_len = strlen(base);
  size_t suffix_len = strlen(suffix);
  bool base_has_slash = base_len > 0U && base[base_len - 1U] == '/';
  size_t need = base_len + (suffix_len && !base_has_slash ? 1U : 0U) +
                suffix_len + 1U;
  if (need > out_sz) return FR_INVALID_NAME;
  strcpy(out, base);
  if (suffix_len) {
    if (!base_has_slash) strcat(out, "/");
    strcat(out, suffix);
  }
  return FR_OK;
}

static bool virtual_write_protected(FMPathKind kind) {
  return kind == FM_PATH_TMP || kind == FM_PATH_DATA ||
         kind == FM_PATH_VIRTUAL_ROOT || kind == FM_PATH_INIT;
}

static void fatal_if_needed(FRESULT res, bool fatal_on_storage_error,
                            uint32_t fallback) {
  if (fatal_on_storage_error && res != FR_OK) {
    SysHandle_FatalFResult(res, fallback);
  }
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

FRESULT FMCore_MountInternal(void) {
  FRESULT res;
  if (g_internal_mounted) return FR_OK;
  if (!FlashDiskIO_EnsureVolumes()) return FR_DISK_ERR;
  res = f_mount(&TMPFatFS, TMPPath, 1U);
  if (res != FR_OK) return res;
  res = f_mount(&DataFatFS, DataPath, 1U);
  if (res != FR_OK) {
    (void)f_mount(NULL, TMPPath, 0U);
    return res;
  }
  g_internal_mounted = true;
  return FR_OK;
}

FRESULT FMCore_MountStorage(FATFS *fs, bool log_result) {
  FRESULT res;
  if (TSDIO_IsHardDisabled() || !TSDIO_IsInitialized()) return FR_NOT_READY;
  if (g_storage_mounted) return FR_OK;
  res = f_mount(fs ? fs : &g_storage_fs, "0:", 1U);
  if (res == FR_OK) g_storage_mounted = true;
  if (log_result) {
    LOG_I("FMCR", "storage mount => %s(%d)", FMCore_FResultName(res), (int)res);
  }
  return res;
}

FRESULT FMCore_Mount(FATFS *fs, bool fatal_on_storage_error) {
  FRESULT internal = FMCore_MountInternal();
  FRESULT storage = FMCore_MountStorage(fs, true);
  if (internal != FR_OK) {
    fatal_if_needed(internal, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
    return internal;
  }
  /* An absent SD leaves /storage empty; it must not make /data or /tmp fail. */
  if (storage != FR_OK && storage != FR_NOT_READY) {
    LOG_W("FMCR", "external storage unavailable: %s(%d)",
          FMCore_FResultName(storage), (int)storage);
  }
  return FR_OK;
}

void FMCore_Unmount(void) {
  if (g_storage_mounted) {
    (void)f_mount(NULL, "0:", 0U);
    g_storage_mounted = false;
  }
}

bool FMCore_IsStorageMounted(void) { return g_storage_mounted; }

static void fill_virtual_entry(FMCore_Entry *entry, const char *name,
                               uint8_t is_dir, uint32_t size) {
  strncpy(entry->name, name, FMCORE_NAME_MAX - 1U);
  entry->name[FMCORE_NAME_MAX - 1U] = '\0';
  entry->is_dir = is_dir;
  entry->size = size;
  entry->attr = is_dir ? AM_DIR : AM_RDO;
}

FRESULT FMCore_Stat(const char *path, FILINFO *info, bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FRESULT res = translate_path(path, physical, sizeof(physical), &kind);
  if (res != FR_OK) return res;
  if (!info) return FR_INVALID_PARAMETER;
  if (kind == FM_PATH_VIRTUAL_ROOT ||
      (kind == FM_PATH_STORAGE && strcmp(path, "/storage") == 0) ||
      (kind == FM_PATH_TMP && strcmp(path, "/tmp") == 0) ||
      (kind == FM_PATH_DATA && strcmp(path, "/data") == 0)) {
    memset(info, 0, sizeof(*info));
    info->fattrib = AM_DIR | (kind == FM_PATH_TMP || kind == FM_PATH_DATA ? AM_RDO : 0U);
    return FR_OK;
  }
  if ((kind == FM_PATH_STORAGE || kind == FM_PATH_INIT) && !g_storage_mounted) {
    return FR_NO_PATH;
  }
  res = f_stat(physical, info);
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

bool FMCore_JoinPath(const char *base, const char *name, char *out, size_t out_sz) {
  size_t base_len;
  size_t name_len;
  bool slash;
  if (!base || !name || !out || out_sz == 0U) return false;
  base_len = strlen(base);
  name_len = strlen(name);
  slash = base_len > 0U && base[base_len - 1U] != '/';
  if (base_len + (slash ? 1U : 0U) + name_len + 1U > out_sz) return false;
  strcpy(out, base);
  if (slash) strcat(out, "/");
  strcat(out, name);
  return true;
}

FRESULT FMCore_ListDir(const char *path, FMCore_Entry *entries, uint16_t max_entries,
                       uint16_t *out_count, bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  DIR dir;
  FILINFO fno;
  uint16_t count = 0U;
  FRESULT res;
  if (out_count) *out_count = 0U;
  if (!path || !entries || max_entries == 0U) return FR_INVALID_PARAMETER;
  res = translate_path(path, physical, sizeof(physical), &kind);
  if (res != FR_OK) return res;

  if (kind == FM_PATH_VIRTUAL_ROOT) {
    static const char *const names[] = {"data", "storage", "tmp", "init"};
    for (uint16_t i = 0U; i < 4U && count < max_entries; ++i) {
      if (i == 3U && !FMCore_IsInitialized()) continue;
      fill_virtual_entry(&entries[count++], names[i], i == 3U ? 0U : 1U,
                         i == 3U ? TOS_SD_INIT_FILE_SIZE : 0U);
    }
    if (out_count) *out_count = count;
    return FR_OK;
  }
  if (kind == FM_PATH_STORAGE && !g_storage_mounted) {
    if (out_count) *out_count = 0U;
    return FR_OK;
  }
  if (kind == FM_PATH_INIT) return FR_INVALID_OBJECT;

  res = f_opendir(&dir, physical);
  if (res != FR_OK) {
    fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
    return res;
  }
  while (count < max_entries) {
    res = f_readdir(&dir, &fno);
    if (res != FR_OK || fno.fname[0] == '\0') break;
    fill_virtual_entry(&entries[count], fno.fname,
                       (fno.fattrib & AM_DIR) ? 1U : 0U, fno.fsize);
    entries[count].attr = fno.fattrib;
    ++count;
  }
  (void)f_closedir(&dir);
  if (out_count) *out_count = count;
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_BROWSER_FAILED);
  return res;
}

static FRESULT translate_writable(const char *path, char *physical,
                                  FMPathKind *kind) {
  FRESULT res = translate_path(path, physical, FMCORE_PATH_MAX, kind);
  if (res != FR_OK) return res;
  return virtual_write_protected(*kind) ? FR_WRITE_PROTECTED : FR_OK;
}

FRESULT FMCore_CreateDir(const char *path, bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FRESULT res = translate_writable(path, physical, &kind);
  if (res == FR_OK) res = f_mkdir(physical);
  if (res == FR_EXIST) res = FR_OK;
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_CreateFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FIL fp;
  UINT bw = 0U;
  FRESULT res = translate_writable(path, physical, &kind);
  if (res != FR_OK) goto done;
  res = f_open(&fp, physical, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) goto done;
  if (len > 0U && data) {
    SysWatchdog_Tick();
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  }
  if (res == FR_OK) res = f_sync(&fp);
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
done:
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_WriteFile(const char *path, const void *data, uint32_t len,
                         bool fatal_on_storage_error) {
  return FMCore_CreateFile(path, data, len, fatal_on_storage_error);
}

FRESULT FMCore_ReadFile(const char *path, void *buf, uint32_t max_len,
                        uint32_t *out_len, bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FIL fp;
  UINT br = 0U;
  FRESULT res;
  if (out_len) *out_len = 0U;
  if (!buf || max_len == 0U) return FR_INVALID_PARAMETER;
  res = translate_path(path, physical, sizeof(physical), &kind);
  if (res != FR_OK || kind == FM_PATH_VIRTUAL_ROOT) return FR_INVALID_OBJECT;
  res = f_open(&fp, physical, FA_READ);
  if (res == FR_OK) {
    res = f_read(&fp, buf, (UINT)max_len, &br);
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
  if (out_len) *out_len = br;
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_AppendFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FIL fp;
  UINT bw = 0U;
  FRESULT res = translate_writable(path, physical, &kind);
  if (res != FR_OK) goto done;
  res = f_open(&fp, physical, FA_OPEN_APPEND | FA_WRITE);
  if (res != FR_OK) goto done;
  if (len > 0U) {
    res = f_write(&fp, data, (UINT)len, &bw);
    if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  }
  if (res == FR_OK) res = f_sync(&fp);
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
done:
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

static bool op_timed_out(uint32_t start, uint32_t timeout_ms) {
  return timeout_ms && (uint32_t)(HAL_GetTick() - start) > timeout_ms;
}

static FRESULT append_physical_timed(const char *path, const void *data,
                                     uint32_t len, uint32_t timeout_ms) {
  FIL fp;
  UINT bw = 0U;
  FRESULT res;
  uint32_t start = HAL_GetTick();
  if (!path || !data || len == 0U) return FR_INVALID_PARAMETER;
  if (!g_storage_mounted) return FR_NOT_READY;
  res = f_open(&fp, path, FA_OPEN_APPEND | FA_WRITE);
  if (res != FR_OK) return res;
  res = f_write(&fp, data, (UINT)len, &bw);
  if (res == FR_OK && bw != len) res = FR_DISK_ERR;
  if (res == FR_OK && !op_timed_out(start, timeout_ms)) res = f_sync(&fp);
  if (res == FR_OK && op_timed_out(start, timeout_ms)) res = FR_TIMEOUT;
  {
    FRESULT close_res = f_close(&fp);
    if (res == FR_OK) res = close_res;
  }
  SysWatchdog_FeedNow();
  return res;
}

static FRESULT delete_physical_recursive(const char *path) {
  FILINFO info;
  FRESULT res = f_stat(path, &info);
  if (res != FR_OK) return res;
  if ((info.fattrib & AM_DIR) == 0U) return f_unlink(path);
  for (;;) {
    DIR dir;
    FILINFO child;
    char full[FMCORE_PATH_MAX];
    bool found = false;
    res = f_opendir(&dir, path);
    if (res != FR_OK) return res;
    while ((res = f_readdir(&dir, &child)) == FR_OK && child.fname[0]) {
      if (!strcmp(child.fname, ".") || !strcmp(child.fname, "..")) continue;
      if (!FMCore_JoinPath(path, child.fname, full, sizeof(full))) res = FR_INVALID_NAME;
      else found = true;
      break;
    }
    (void)f_closedir(&dir);
    if (res != FR_OK) return res;
    if (!found) break;
    res = delete_physical_recursive(full);
    if (res != FR_OK) return res;
    SysWatchdog_Tick();
  }
  return f_unlink(path);
}

FRESULT FMCore_Delete(const char *path, bool recursive, bool fatal_on_storage_error) {
  char physical[FMCORE_PATH_MAX];
  FMPathKind kind;
  FILINFO info;
  FRESULT res = translate_writable(path, physical, &kind);
  if (res != FR_OK) goto done;
  res = f_stat(physical, &info);
  if (res != FR_OK) goto done;
  if ((info.fattrib & AM_DIR) && recursive) res = delete_physical_recursive(physical);
  else res = f_unlink(physical);
done:
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

FRESULT FMCore_CopyFile(const char *src, const char *dst, bool fatal_on_storage_error) {
  char src_physical[FMCORE_PATH_MAX];
  char dst_physical[FMCORE_PATH_MAX];
  FMPathKind src_kind, dst_kind;
  FIL in, out;
  uint8_t buf[256];
  UINT br = 0U, bw = 0U;
  FRESULT res = translate_path(src, src_physical, sizeof(src_physical), &src_kind);
  if (res != FR_OK) goto done;
  res = translate_writable(dst, dst_physical, &dst_kind);
  if (res != FR_OK) goto done;
  res = f_open(&in, src_physical, FA_READ);
  if (res != FR_OK) goto done;
  res = f_open(&out, dst_physical, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK) { (void)f_close(&in); goto done; }
  do {
    res = f_read(&in, buf, sizeof(buf), &br);
    if (res != FR_OK || br == 0U) break;
    res = f_write(&out, buf, br, &bw);
    if (res == FR_OK && bw != br) res = FR_DISK_ERR;
  } while (res == FR_OK && br == sizeof(buf));
  if (res == FR_OK) res = f_sync(&out);
  (void)f_close(&out);
  (void)f_close(&in);
done:
  fatal_if_needed(res, fatal_on_storage_error, SYS_ERR_SD_FILE_OP_FAILED);
  return res;
}

bool FMCore_IsInitialized(void) {
  FILINFO info;
  if (!g_storage_mounted) return false;
  if (f_stat("0:/init", &info) != FR_OK || (info.fattrib & AM_DIR) != 0U) return false;
  return info.fsize == TOS_SD_INIT_FILE_SIZE;
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

static FRESULT ensure_tos_dir(const char *leaf_dir) {
  FRESULT res;
  if (!FMCore_IsInitialized()) return FR_NOT_READY;
  res = f_mkdir("0:/storage");
  if (res != FR_OK && res != FR_EXIST) return res;
  res = f_mkdir("0:/storage/tos");
  if (res != FR_OK && res != FR_EXIST) return res;
  res = f_mkdir(leaf_dir);
  return res == FR_EXIST ? FR_OK : res;
}

FRESULT FMCore_AppendBootLog(const char *line, uint32_t len) {
  FRESULT res;
  if (!line || len == 0U) return FR_INVALID_PARAMETER;
  if (!g_boot_log_ready_seen) {
    res = ensure_tos_dir(FMCORE_LOG_DIR);
    if (res != FR_OK) return res;
    g_boot_log_ready_seen = true;
  }
  if (g_boot_log_path[0] == '\0') {
    res = FMCore_NextIndexedPath(FMCORE_LOG_DIR, "log", g_boot_log_path,
                                 sizeof(g_boot_log_path));
    if (res != FR_OK) return res;
  }
  return append_physical_timed(g_boot_log_path, line, len,
                               FMCORE_BOOT_LOG_TIMEOUT_MS);
}

bool FMCore_IsBootLogFaultFatal(void) { return false; }

FRESULT FMCore_WriteSystemDump(uint32_t code, const char *name,
                               const char *extra) {
  char path[FMCORE_PATH_MAX];
  char body[512];
  int n;
  FRESULT res = ensure_tos_dir(FMCORE_DUMP_DIR);
  if (res != FR_OK) return res;
  res = FMCore_NextIndexedPath(FMCORE_DUMP_DIR, "dump", path, sizeof(path));
  if (res != FR_OK) return res;
  n = snprintf(body, sizeof(body),
               "TOS system dump\r\ncode=0x%08lX\r\ntype=%s\r\n%s%s",
               (unsigned long)code, name ? name : "UNKNOWN",
               extra ? extra : "", (extra && extra[0]) ? "" : "\r\n");
  if (n <= 0) return FR_INVALID_PARAMETER;
  if (n >= (int)sizeof(body)) n = (int)sizeof(body) - 1;
  return FMCore_CreateFile(path, body, (uint32_t)n, false);
}
