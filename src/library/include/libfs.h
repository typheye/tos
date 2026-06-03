/**
 ******************************************************************************
 * @file    libfs.h
 * @author  Typheye
 * @brief   Libfs interface.
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

#ifndef __LIBFS_H
#define __LIBFS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 文件打开模式
typedef enum {
  FS_MODE_READ = 0x01,          // 只读
  FS_MODE_WRITE = 0x02,         // 只写
  FS_MODE_READ_WRITE = 0x03,    // 读写
  FS_MODE_CREATE_ALWAYS = 0x04, // 总是创建新文件
  FS_MODE_CREATE_NEW = 0x08,    // 创建新文件 (已存在则失败)
  FS_MODE_OPEN_ALWAYS = 0x10,   // 打开或创建
  FS_MODE_APPEND = 0x20         // 追加模式
} FS_Mode_t;

// 文件指针移动方式
typedef enum {
  FS_SEEK_SET = 0, // 从文件开头
  FS_SEEK_CUR = 1, // 从当前位置
  FS_SEEK_END = 2  // 从文件末尾
} FS_Seek_t;

// 文件系统状态
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

// 文件信息结构体
typedef struct {
  uint32_t size;  // 文件大小 (字节)
  uint16_t date;  // 修改日期
  uint16_t time;  // 修改时间
  uint8_t attrib; // 文件属性
  char name[256]; // 文件名
} FS_FileInfo_t;

// 文件操作句柄 (不透明指针)
typedef void *FS_FileHandle;

// 目录操作句柄
typedef void *FS_DirHandle;

// ========== 文件系统初始化和管理 ==========

// 初始化文件系统
FS_Status_t FS_Init(void);

// 挂载 SD 卡
FS_Status_t FS_Mount(const char *path);

// 卸载 SD 卡
FS_Status_t FS_Unmount(const char *path);

// 格式化 SD 卡 (FAT32)
FS_Status_t FS_Format(const char *path);

// 获取文件系统状态
FS_Status_t FS_GetStatus(const char *path);

// 获取 SD 卡容量信息
FS_Status_t FS_GetVolumeInfo(const char *path, uint32_t *total_mb,
                             uint32_t *free_mb);

// ========== 文件和目录操作 ==========

// 打开文件
FS_Status_t FS_Open(FS_FileHandle *file, const char *path, FS_Mode_t mode);

// 关闭文件
FS_Status_t FS_Close(FS_FileHandle file);

// 读取文件
FS_Status_t FS_Read(FS_FileHandle file, void *buffer, uint32_t size,
                    uint32_t *bytes_read);

// 写入文件
FS_Status_t FS_Write(FS_FileHandle file, const void *buffer, uint32_t size,
                     uint32_t *bytes_written);

// 移动文件指针
FS_Status_t FS_Seek(FS_FileHandle file, uint32_t offset, FS_Seek_t whence);

// 获取文件指针位置
FS_Status_t FS_Tell(FS_FileHandle file, uint32_t *offset);

// 获取文件大小
FS_Status_t FS_GetFileSize(FS_FileHandle file, uint32_t *size);

// 截断文件
FS_Status_t FS_Truncate(FS_FileHandle file);

// 同步文件缓冲区到磁盘
FS_Status_t FS_Sync(FS_FileHandle file);

// 检查文件是否结束
bool FS_Eof(FS_FileHandle file);

// ========== 目录操作 ==========

// 打开目录
FS_Status_t FS_OpenDir(FS_DirHandle *dir, const char *path);

// 读取目录条目
FS_Status_t FS_ReadDir(FS_DirHandle dir, FS_FileInfo_t *info);

// 关闭目录
FS_Status_t FS_CloseDir(FS_DirHandle dir);

// 创建目录
FS_Status_t FS_CreateDir(const char *path);

// 删除目录 (必须为空)
FS_Status_t FS_RemoveDir(const char *path);

// ========== 文件操作 ==========

// 删除文件
FS_Status_t FS_Remove(const char *path);

// 重命名/移动文件
FS_Status_t FS_Rename(const char *old_path, const char *new_path);

// 检查文件/目录是否存在
bool FS_Exists(const char *path);

// 获取文件/目录信息
FS_Status_t FS_GetInfo(const char *path, FS_FileInfo_t *info);

// 获取当前工作目录
FS_Status_t FS_GetWorkingDir(char *buffer, uint32_t size);

// 改变当前工作目录
FS_Status_t FS_ChangeDir(const char *path);

// ========== 便捷函数 ==========

// 创建并写入文件 (一次性写入)
FS_Status_t FS_WriteFile(const char *path, const void *data, uint32_t size);

// 读取整个文件 (需要释放返回的buffer)
void *FS_ReadFile(const char *path, uint32_t *size);

// 追加内容到文件
FS_Status_t FS_AppendFile(const char *path, const void *data, uint32_t size);

// 列出目录内容
FS_Status_t FS_ListDir(const char *path, FS_FileInfo_t *infos,
                       uint32_t max_count, uint32_t *actual_count);

// 获取文件扩展名
const char *FS_GetExtension(const char *filename);

// 检查路径是否为目录
bool FS_IsDirectory(const char *path);

// 检查路径是否为文件
bool FS_IsFile(const char *path);

// ========== 错误处理 ==========

// 获取最后一次错误码
FS_Status_t FS_GetLastError(void);

// 转换错误码为字符串
const char *FS_ErrorToString(FS_Status_t error);

#ifdef __cplusplus
}
#endif

#endif // __LIBFS_H