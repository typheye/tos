/**
 ******************************************************************************
 * @file    file_manager.h
 * @author  Typheye
 * @brief   File Manager interface.
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

#ifndef __CORE_FILE_MANAGER_H
#define __CORE_FILE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ff.h"

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
void FMCore_Unmount(void);
FRESULT FMCore_Stat(const char *path, FILINFO *info, bool fatal_on_storage_error);
FRESULT FMCore_ListDir(const char *path, FMCore_Entry *entries, uint16_t max_entries,
                       uint16_t *out_count, bool fatal_on_storage_error);
FRESULT FMCore_CreateDir(const char *path, bool fatal_on_storage_error);
FRESULT FMCore_CreateFile(const char *path, const void *data, uint32_t len,
                          bool fatal_on_storage_error);
FRESULT FMCore_Delete(const char *path, bool recursive, bool fatal_on_storage_error);
FRESULT FMCore_CopyFile(const char *src, const char *dst, bool fatal_on_storage_error);
FRESULT FMCore_InitLayout(bool fatal_on_storage_error);

const char *FMCore_FResultName(FRESULT res);
bool FMCore_JoinPath(const char *base, const char *name, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* __CORE_FILE_MANAGER_H */
