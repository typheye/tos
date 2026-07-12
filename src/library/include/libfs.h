/**
 ******************************************************************************
 * @file    libfs.h
 * @author  Typheye
 * @brief   Libfs interface.
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

#ifndef LIBFS_H
#define LIBFS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "diskio.h"
#include "ff.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
  FS_MODE_READ = 0x01,
  FS_MODE_WRITE = 0x02,
  FS_MODE_READ_WRITE = 0x03,
  FS_MODE_CREATE_ALWAYS = 0x04,
  FS_MODE_CREATE_NEW = 0x08,
  FS_MODE_OPEN_ALWAYS = 0x10,
  FS_MODE_APPEND = 0x20
} FS_Mode_t;


typedef enum {
  FS_SEEK_SET = 0,
  FS_SEEK_CUR = 1,
  FS_SEEK_END = 2
} FS_Seek_t;


typedef enum {
  FS_OK = 0,
  FS_ERROR = 1,
  FS_NOT_MOUNTED = 2,
  FS_NO_FILESYSTEM = 3,
  FS_DISK_ERR = 4,
  FS_INT_ERR = 5,
  FS_NOT_READY = 6,
  FS_NO_FILE = 7,
  FS_NO_PATH = 8,
  FS_INVALID_NAME = 9,
  FS_DENIED = 10,
  FS_EXIST = 11,
  FS_INVALID_OBJECT = 12,
  FS_WRITE_PROTECTED = 13,
  FS_INVALID_DRIVE = 14,
  FS_NOT_ENABLED = 15,
  FS_NO_FILESYSTEM_ALT = 16,
  FS_MKFS_ABORTED = 17,
  FS_TIMEOUT = 18,
  FS_LOCKED = 19,
  FS_NOT_ENOUGH_CORE = 20,
  FS_TOO_MANY_OPEN_FILES = 21,
  FS_INVALID_PARAMETER = 22
} FS_Status_t;


typedef struct {
  uint32_t size;
  uint16_t date;
  uint16_t time;
  uint8_t attrib;
  char name[256];
} FS_FileInfo_t;


typedef void *FS_FileHandle;


typedef void *FS_DirHandle;




FS_Status_t FS_Init(void);


FS_Status_t FS_Mount(const char *path);


FS_Status_t FS_Unmount(const char *path);


FS_Status_t FS_Format(const char *path);


FS_Status_t FS_GetStatus(const char *path);


FS_Status_t FS_GetVolumeInfo(const char *path, uint32_t *total_mb,
                             uint32_t *free_mb);




FS_Status_t FS_Open(FS_FileHandle *file, const char *path, FS_Mode_t mode);


FS_Status_t FS_Close(FS_FileHandle file);


FS_Status_t FS_Read(FS_FileHandle file, void *buffer, uint32_t size,
                    uint32_t *bytes_read);


FS_Status_t FS_Write(FS_FileHandle file, const void *buffer, uint32_t size,
                     uint32_t *bytes_written);


FS_Status_t FS_Seek(FS_FileHandle file, uint32_t offset, FS_Seek_t whence);


FS_Status_t FS_Tell(FS_FileHandle file, uint32_t *offset);


FS_Status_t FS_GetFileSize(FS_FileHandle file, uint32_t *size);


FS_Status_t FS_Truncate(FS_FileHandle file);


FS_Status_t FS_Sync(FS_FileHandle file);


bool FS_Eof(FS_FileHandle file);




FS_Status_t FS_OpenDir(FS_DirHandle *dir, const char *path);


FS_Status_t FS_ReadDir(FS_DirHandle dir, FS_FileInfo_t *info);


FS_Status_t FS_CloseDir(FS_DirHandle dir);


FS_Status_t FS_CreateDir(const char *path);


FS_Status_t FS_RemoveDir(const char *path);




FS_Status_t FS_Remove(const char *path);


FS_Status_t FS_Rename(const char *old_path, const char *new_path);


bool FS_Exists(const char *path);


FS_Status_t FS_GetInfo(const char *path, FS_FileInfo_t *info);


FS_Status_t FS_GetWorkingDir(char *buffer, uint32_t size);


FS_Status_t FS_ChangeDir(const char *path);




FS_Status_t FS_WriteFile(const char *path, const void *data, uint32_t size);


void *FS_ReadFile(const char *path, uint32_t *size);


FS_Status_t FS_AppendFile(const char *path, const void *data, uint32_t size);


FS_Status_t FS_ListDir(const char *path, FS_FileInfo_t *infos,
                       uint32_t max_count, uint32_t *actual_count);


const char *FS_GetExtension(const char *filename);


bool FS_IsDirectory(const char *path);


bool FS_IsFile(const char *path);




FS_Status_t FS_GetLastError(void);


const char *FS_ErrorToString(FS_Status_t error);

#ifdef __cplusplus
}
#endif

#endif // LIBFS_H