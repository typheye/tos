/**
 ******************************************************************************
 * @file    libfs.c
 * @author  Typheye
 * @brief   Libfs implementation.
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

#include "include/libfs.h"
#include "core/sys/include/sysdram.h"

/* C-compatible SD hard-disabled check (defined in hardware/tsdio.cpp) */
extern bool TSDIO_IsHardDisabled(void);

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif


static CCMRAM FATFS fs;


static CCMRAM char current_dir[256] = "0:";


static CCMRAM FS_Status_t last_error = FS_OK;


static CCMRAM bool is_mounted = false;

static uint8_t fs_dma_scratch[512]
    __attribute__((section(".sysdram_core_fixed"), aligned(4), used));


static FS_Status_t fatfs_error_to_fs(FRESULT res) {
  switch (res) {
  case FR_OK:
    return FS_OK;
  case FR_DISK_ERR:
    return FS_DISK_ERR;
  case FR_INT_ERR:
    return FS_INT_ERR;
  case FR_NOT_READY:
    return FS_NOT_READY;
  case FR_NO_FILE:
    return FS_NO_FILE;
  case FR_NO_PATH:
    return FS_NO_PATH;
  case FR_INVALID_NAME:
    return FS_INVALID_NAME;
  case FR_DENIED:
    return FS_DENIED;
  case FR_EXIST:
    return FS_EXIST;
  case FR_INVALID_OBJECT:
    return FS_INVALID_OBJECT;
  case FR_WRITE_PROTECTED:
    return FS_WRITE_PROTECTED;
  case FR_INVALID_DRIVE:
    return FS_INVALID_DRIVE;
  case FR_NOT_ENABLED:
    return FS_NOT_ENABLED;
  case FR_NO_FILESYSTEM:
    return FS_NO_FILESYSTEM;
  case FR_MKFS_ABORTED:
    return FS_MKFS_ABORTED;
  case FR_TIMEOUT:
    return FS_TIMEOUT;
  case FR_LOCKED:
    return FS_LOCKED;
  case FR_NOT_ENOUGH_CORE:
    return FS_NOT_ENOUGH_CORE;
  case FR_TOO_MANY_OPEN_FILES:
    return FS_TOO_MANY_OPEN_FILES;
  case FR_INVALID_PARAMETER:
    return FS_INVALID_PARAMETER;
  default:
    return FS_ERROR;
  }
}


static BYTE fatfs_open_mode(FS_Mode_t mode) {
  BYTE fatfs_mode = 0;

  if (mode & FS_MODE_READ) {
    fatfs_mode |= FA_READ;
  }
  if (mode & FS_MODE_WRITE) {
    fatfs_mode |= FA_WRITE;
  }
  if (mode & FS_MODE_CREATE_ALWAYS) {
    fatfs_mode |= FA_CREATE_ALWAYS;
  }
  if (mode & FS_MODE_CREATE_NEW) {
    fatfs_mode |= FA_CREATE_NEW;
  }
  if (mode & FS_MODE_OPEN_ALWAYS) {
    fatfs_mode |= FA_OPEN_ALWAYS;
  }
  if (mode & FS_MODE_APPEND) {
    fatfs_mode |= FA_OPEN_APPEND;
  }

  if (fatfs_mode == 0) {
    fatfs_mode = FA_READ;
  }

  return fatfs_mode;
}


static void fatfs_to_file_info(FILINFO *fat_info, FS_FileInfo_t *info) {
  if (!fat_info || !info)
    return;

  info->size = fat_info->fsize;
  info->date = fat_info->fdate;
  info->time = fat_info->ftime;
  info->attrib = fat_info->fattrib;
  strncpy(info->name, fat_info->fname, sizeof(info->name) - 1);
  info->name[sizeof(info->name) - 1] = '\0';
}



FS_Status_t FS_Init(void) {
  last_error = FS_OK;
  return FS_OK;
}

FS_Status_t FS_Mount(const char *path) {
  FRESULT res;
  const char *drive = path ? path : "0:";

  /* If SD card is hard-disabled, return immediately. */
  if (TSDIO_IsHardDisabled()) {
    last_error = FS_ERROR;
    return FS_ERROR;
  }

  res = f_mount(&fs, drive, 1);

  if (res == FR_OK) {
    is_mounted = true;
    if (path) {
      strcpy(current_dir, path);
    } else {
      strcpy(current_dir, "0:");
    }
    last_error = FS_OK;
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Unmount(const char *path) {
  FRESULT res;
  const char *drive = path ? path : "0:";

  res = f_mount(NULL, drive, 0);

  if (res == FR_OK) {
    is_mounted = false;
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Format(const char *path) {
  FRESULT res;
  const char *drive = path ? path : "0:";

  /* If SD card is hard-disabled, return immediately. */
  if (TSDIO_IsHardDisabled()) {
    last_error = FS_ERROR;
    return FS_ERROR;
  }

#ifdef FF_MAX_SS
  uint32_t work_buffer[FF_MAX_SS / sizeof(uint32_t)];
#else
  uint32_t work_buffer[512 / sizeof(uint32_t)];
#endif

  f_mount(NULL, drive, 0);
  res = f_mkfs(drive, 0, 0, work_buffer, sizeof(work_buffer));

  if (res == FR_OK) {
    return FS_Mount(drive);
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_GetStatus(const char *path) {
  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  DSTATUS status = disk_status(0);

  if (status & STA_NOINIT) {
    return FS_NOT_READY;
  }
  if (status & STA_NODISK) {
    return FS_DISK_ERR;
  }
  if (status & STA_PROTECT) {
    return FS_WRITE_PROTECTED;
  }

  return FS_OK;
}

FS_Status_t FS_GetVolumeInfo(const char *path, uint32_t *total_mb,
                             uint32_t *free_mb) {
  FRESULT res;
  DWORD free_clusters, total_clusters, sectors_per_cluster;
  FATFS *fs_temp;
  const char *drive = path ? path : current_dir;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  res = f_getfree(drive, &free_clusters, &fs_temp);
  if (res != FR_OK) {
    last_error = fatfs_error_to_fs(res);
    return last_error;
  }

  sectors_per_cluster = fs_temp->csize;
  total_clusters = (fs_temp->n_fatent - 2);

  if (total_mb) {
    *total_mb = (total_clusters * sectors_per_cluster * 512) / (1024 * 1024);
  }
  if (free_mb) {
    *free_mb = (free_clusters * sectors_per_cluster * 512) / (1024 * 1024);
  }

  return FS_OK;
}



FS_Status_t FS_Open(FS_FileHandle *file, const char *path, FS_Mode_t mode) {
  FRESULT res;
  FIL *fil = NULL;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!file || !path) {
    return FS_INVALID_PARAMETER;
  }

  fil = (FIL *)malloc(sizeof(FIL));
  if (!fil) {
    return FS_NOT_ENOUGH_CORE;
  }

  res = f_open(fil, path, fatfs_open_mode(mode));

  if (res == FR_OK) {
    *file = (FS_FileHandle)fil;
    return FS_OK;
  }

  free(fil);
  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Close(FS_FileHandle file) {
  FRESULT res;
  FIL *fil = (FIL *)file;

  if (!fil) {
    return FS_INVALID_OBJECT;
  }

  res = f_close(fil);
  free(fil);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Read(FS_FileHandle file, void *buffer, uint32_t size,
                    uint32_t *bytes_read) {
  FRESULT res;
  FIL *fil = (FIL *)file;
  UINT br = 0;

  if (!fil || !buffer) {
    return FS_INVALID_PARAMETER;
  }

  if (!SysDram_IsCcmPtr(buffer)) {
    res = f_read(fil, buffer, size, &br);
  } else {
    uint8_t *dst = (uint8_t *)buffer;
    uint32_t done = 0;
    res = FR_OK;
    while (done < size) {
      UINT got = 0;
      uint32_t chunk = size - done;
      if (chunk > sizeof(fs_dma_scratch)) {
        chunk = sizeof(fs_dma_scratch);
      }
      res = f_read(fil, fs_dma_scratch, (UINT)chunk, &got);
      if (got > 0U) {
        memcpy(dst + done, fs_dma_scratch, got);
        done += got;
      }
      if (res != FR_OK || got < chunk) {
        break;
      }
    }
    br = (UINT)done;
  }

  if (bytes_read) {
    *bytes_read = br;
  }

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Write(FS_FileHandle file, const void *buffer, uint32_t size,
                     uint32_t *bytes_written) {
  FRESULT res;
  FIL *fil = (FIL *)file;
  UINT bw = 0;

  if (!fil || !buffer) {
    return FS_INVALID_PARAMETER;
  }

  if (!SysDram_IsCcmPtr(buffer)) {
    res = f_write(fil, buffer, size, &bw);
  } else {
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t done = 0;
    res = FR_OK;
    while (done < size) {
      UINT wrote = 0;
      uint32_t chunk = size - done;
      if (chunk > sizeof(fs_dma_scratch)) {
        chunk = sizeof(fs_dma_scratch);
      }
      memcpy(fs_dma_scratch, src + done, chunk);
      res = f_write(fil, fs_dma_scratch, (UINT)chunk, &wrote);
      done += wrote;
      if (res != FR_OK || wrote < chunk) {
        break;
      }
    }
    bw = (UINT)done;
  }

  if (bytes_written) {
    *bytes_written = bw;
  }

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Seek(FS_FileHandle file, uint32_t offset, FS_Seek_t whence) {
  FRESULT res;
  FIL *fil = (FIL *)file;
  uint32_t new_pos = 0;

  if (!fil) {
    return FS_INVALID_OBJECT;
  }

  switch (whence) {
  case FS_SEEK_SET:
    new_pos = offset;
    break;
  case FS_SEEK_CUR:
    new_pos = f_tell(fil) + offset;
    break;
  case FS_SEEK_END:
    new_pos = f_size(fil) + offset;
    break;
  default:
    return FS_INVALID_PARAMETER;
  }

  res = f_lseek(fil, new_pos);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Tell(FS_FileHandle file, uint32_t *offset) {
  FIL *fil = (FIL *)file;

  if (!fil || !offset) {
    return FS_INVALID_PARAMETER;
  }

  *offset = f_tell(fil);
  return FS_OK;
}

FS_Status_t FS_GetFileSize(FS_FileHandle file, uint32_t *size) {
  FIL *fil = (FIL *)file;

  if (!fil || !size) {
    return FS_INVALID_PARAMETER;
  }

  *size = f_size(fil);
  return FS_OK;
}

FS_Status_t FS_Truncate(FS_FileHandle file) {
  FRESULT res;
  FIL *fil = (FIL *)file;

  if (!fil) {
    return FS_INVALID_OBJECT;
  }

  res = f_truncate(fil);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Sync(FS_FileHandle file) {
  FRESULT res;
  FIL *fil = (FIL *)file;

  if (!fil) {
    return FS_INVALID_OBJECT;
  }

  res = f_sync(fil);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

bool FS_Eof(FS_FileHandle file) {
  FIL *fil = (FIL *)file;
  if (!fil)
    return true;
  return (f_eof(fil) != 0);
}



FS_Status_t FS_OpenDir(FS_DirHandle *dir, const char *path) {
  FRESULT res;
  DIR *dir_obj = NULL;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!dir || !path) {
    return FS_INVALID_PARAMETER;
  }

  dir_obj = (DIR *)malloc(sizeof(DIR));
  if (!dir_obj) {
    return FS_NOT_ENOUGH_CORE;
  }

  res = f_opendir(dir_obj, path);

  if (res == FR_OK) {
    *dir = (FS_DirHandle)dir_obj;
    return FS_OK;
  }

  free(dir_obj);
  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_ReadDir(FS_DirHandle dir, FS_FileInfo_t *info) {
  FRESULT res;
  DIR *dir_obj = (DIR *)dir;
  FILINFO fat_info;

  if (!dir_obj || !info) {
    return FS_INVALID_PARAMETER;
  }

  res = f_readdir(dir_obj, &fat_info);

  if (res == FR_OK) {
    if (fat_info.fname[0] == 0) {
      return FS_NO_FILE;
    }
    fatfs_to_file_info(&fat_info, info);
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_CloseDir(FS_DirHandle dir) {
  FRESULT res;
  DIR *dir_obj = (DIR *)dir;

  if (!dir_obj) {
    return FS_INVALID_OBJECT;
  }

  res = f_closedir(dir_obj);
  free(dir_obj);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_CreateDir(const char *path) {
  FRESULT res;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!path) {
    return FS_INVALID_PARAMETER;
  }

  res = f_mkdir(path);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_RemoveDir(const char *path) { return FS_Remove(path); }



FS_Status_t FS_Remove(const char *path) {
  FRESULT res;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!path) {
    return FS_INVALID_PARAMETER;
  }

  res = f_unlink(path);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_Rename(const char *old_path, const char *new_path) {
  FRESULT res;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!old_path || !new_path) {
    return FS_INVALID_PARAMETER;
  }

  res = f_rename(old_path, new_path);

  if (res == FR_OK) {
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

bool FS_Exists(const char *path) {
  FRESULT res;
  FILINFO info;

  if (!is_mounted || !path) {
    return false;
  }

  res = f_stat(path, &info);
  return (res == FR_OK);
}

FS_Status_t FS_GetInfo(const char *path, FS_FileInfo_t *info) {
  FRESULT res;
  FILINFO fat_info;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!path || !info) {
    return FS_INVALID_PARAMETER;
  }

  res = f_stat(path, &fat_info);

  if (res == FR_OK) {
    fatfs_to_file_info(&fat_info, info);
    return FS_OK;
  }

  last_error = fatfs_error_to_fs(res);
  return last_error;
}

FS_Status_t FS_GetWorkingDir(char *buffer, uint32_t size) {
  if (!buffer || size == 0) {
    return FS_INVALID_PARAMETER;
  }

  strncpy(buffer, current_dir, size - 1);
  buffer[size - 1] = '\0';

  return FS_OK;
}

FS_Status_t FS_ChangeDir(const char *path) {
  FRESULT res;

  if (!is_mounted) {
    return FS_NOT_MOUNTED;
  }

  if (!path) {
    return FS_INVALID_PARAMETER;
  }

  res = f_stat(path, NULL);
  if (res != FR_OK) {
    last_error = fatfs_error_to_fs(res);
    return last_error;
  }

  strncpy(current_dir, path, sizeof(current_dir) - 1);
  current_dir[sizeof(current_dir) - 1] = '\0';

  return FS_OK;
}



FS_Status_t FS_WriteFile(const char *path, const void *data, uint32_t size) {
  FS_FileHandle file;
  FS_Status_t status;
  uint32_t written;

  if (!path || !data) {
    return FS_INVALID_PARAMETER;
  }

  status = FS_Open(&file, path, FS_MODE_CREATE_ALWAYS | FS_MODE_WRITE);
  if (status != FS_OK) {
    return status;
  }

  status = FS_Write(file, data, size, &written);
  FS_Close(file);

  if (status != FS_OK || written != size) {
    return status != FS_OK ? status : FS_ERROR;
  }

  return FS_OK;
}

void *FS_ReadFile(const char *path, uint32_t *size) {
  FS_FileHandle file;
  FS_Status_t status;
  void *buffer = NULL;
  uint32_t file_size = 0;
  uint32_t bytes_read = 0;

  if (!path) {
    return NULL;
  }

  status = FS_Open(&file, path, FS_MODE_READ);
  if (status != FS_OK) {
    return NULL;
  }

  FS_GetFileSize(file, &file_size);

  buffer = malloc(file_size + 1);
  if (!buffer) {
    FS_Close(file);
    return NULL;
  }

  status = FS_Read(file, buffer, file_size, &bytes_read);
  FS_Close(file);

  if (status != FS_OK || bytes_read != file_size) {
    free(buffer);
    return NULL;
  }

  ((char *)buffer)[file_size] = '\0';

  if (size) {
    *size = file_size;
  }

  return buffer;
}

FS_Status_t FS_AppendFile(const char *path, const void *data, uint32_t size) {
  FS_FileHandle file;
  FS_Status_t status;
  uint32_t written;

  if (!path || !data) {
    return FS_INVALID_PARAMETER;
  }

  status = FS_Open(&file, path, FS_MODE_OPEN_ALWAYS | FS_MODE_WRITE);
  if (status != FS_OK) {
    return status;
  }

  FS_Seek(file, 0, FS_SEEK_END);
  status = FS_Write(file, data, size, &written);
  FS_Close(file);

  if (status != FS_OK) {
    return status;
  }

  return FS_OK;
}

const char *FS_GetExtension(const char *filename) {
  const char *dot;

  if (!filename) {
    return NULL;
  }

  dot = strrchr(filename, '.');
  if (!dot || dot == filename) {
    return "";
  }

  return dot + 1;
}

bool FS_IsDirectory(const char *path) {
  FS_FileInfo_t info;

  if (FS_GetInfo(path, &info) != FS_OK) {
    return false;
  }

  return (info.attrib & AM_DIR) != 0;
}

bool FS_IsFile(const char *path) {
  FS_FileInfo_t info;

  if (FS_GetInfo(path, &info) != FS_OK) {
    return false;
  }

  return (info.attrib & AM_DIR) == 0;
}

FS_Status_t FS_ListDir(const char *path, FS_FileInfo_t *infos,
                       uint32_t max_count, uint32_t *actual_count) {
  FS_DirHandle dir;
  FS_Status_t status;
  uint32_t count = 0;

  if (!path || !infos || !actual_count) {
    return FS_INVALID_PARAMETER;
  }

  status = FS_OpenDir(&dir, path);
  if (status != FS_OK) {
    return status;
  }

  while (count < max_count) {
    status = FS_ReadDir(dir, &infos[count]);
    if (status == FS_NO_FILE) {
      break;
    }
    if (status != FS_OK) {
      FS_CloseDir(dir);
      return status;
    }
    count++;
  }

  FS_CloseDir(dir);
  *actual_count = count;

  return FS_OK;
}



FS_Status_t FS_GetLastError(void) { return last_error; }

const char *FS_ErrorToString(FS_Status_t error) {
  switch (error) {
  case FS_OK:
    return "OK";
  case FS_ERROR:
    return "General error";
  case FS_NOT_MOUNTED:
    return "Filesystem not mounted";
  case FS_NO_FILESYSTEM:
    return "No filesystem found";
  case FS_DISK_ERR:
    return "Disk I/O error";
  case FS_INT_ERR:
    return "Internal error";
  case FS_NOT_READY:
    return "Drive not ready";
  case FS_NO_FILE:
    return "File not found";
  case FS_NO_PATH:
    return "Path not found";
  case FS_INVALID_NAME:
    return "Invalid name";
  case FS_DENIED:
    return "Access denied";
  case FS_EXIST:
    return "File already exists";
  case FS_INVALID_OBJECT:
    return "Invalid object";
  case FS_WRITE_PROTECTED:
    return "Write protected";
  case FS_INVALID_DRIVE:
    return "Invalid drive";
  case FS_NOT_ENABLED:
    return "Drive not enabled";
  case FS_MKFS_ABORTED:
    return "Format aborted";
  case FS_TIMEOUT:
    return "Timeout";
  case FS_LOCKED:
    return "File locked";
  case FS_NOT_ENOUGH_CORE:
    return "Not enough memory";
  case FS_TOO_MANY_OPEN_FILES:
    return "Too many open files";
  case FS_INVALID_PARAMETER:
    return "Invalid parameter";
  default:
    return "Unknown error";
  }
}
