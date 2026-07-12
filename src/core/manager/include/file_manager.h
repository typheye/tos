/**
 ******************************************************************************
 * @file    file_manager.h
 * @author  Typheye
 * @brief   File Manager interface.
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

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "core/include/syshandle.h"
#include "core/sys/include/syslog.h"
#include "core/sys/include/syswatchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FMCORE_PATH_MAX 192U
#define FMCORE_NAME_MAX 64U

typedef struct {
  char name[FMCORE_NAME_MAX];
  uint8_t is_dir;
  uint32_t size;
  uint8_t attr;
} FMCore_Entry;

FRESULT FMCore_Mount(FATFS *fs, bool fatal_on_storage_error);
FRESULT FMCore_MountInternal(void);
FRESULT FMCore_MountStorage(FATFS *fs, bool log_result);
void FMCore_Unmount(void);
bool FMCore_IsStorageMounted(void);
FRESULT FMCore_Stat(const char *path, FILINFO *info, bool fatal_on_storage_error);
FRESULT FMCore_ListDir(const char *path, FMCore_Entry *entries, uint16_t max_entries,
                       uint16_t *out_count, bool fatal_on_storage_error);
FRESULT FMCore_CreateDir(const char *path, bool fatal_on_storage_error);
FRESULT FMCore_CreateFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error);
FRESULT FMCore_WriteFile(const char *path, const void *data, uint32_t len,
                         bool fatal_on_storage_error);
FRESULT FMCore_ReadFile(const char *path, void *buf, uint32_t max_len,
                        uint32_t *out_len, bool fatal_on_storage_error);
FRESULT FMCore_AppendFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error);
FRESULT FMCore_Delete(const char *path, bool recursive, bool fatal_on_storage_error);
FRESULT FMCore_CopyFile(const char *src, const char *dst, bool fatal_on_storage_error);
bool FMCore_IsInitialized(void);
FRESULT FMCore_NextIndexedPath(const char *dir, const char *ext,
                               char *out, size_t out_sz);
FRESULT FMCore_PrepareSystemStorage(void);
FRESULT FMCore_AppendBootLog(const char *line, uint32_t len);
bool FMCore_IsBootLogFaultFatal(void);
FRESULT FMCore_WriteSystemDump(uint32_t code, const char *name,
                               const char *extra);

const char *FMCore_FResultName(FRESULT res);
bool FMCore_JoinPath(const char *base, const char *name, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* FILE_MANAGER_H */
