/**
 ******************************************************************************
 * @file    sfhd.h
 * @author  Typheye
 * @brief   Sfhd interface.
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

#ifndef __SFHD_H
#define __SFHD_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 地址与扇区定义 ========== */
#define FLASH_DATA_SECTOR    FLASH_SECTOR_11   /* 数据存储扇区 */
#define FLASH_DATA_ADDR      0x080E0000u       /* 数据区起始地址 */
#define FLASH_DATA_SIZE      0x20000u          /* 扇区大小 128KB */

#define FLASH_BACKUP_SECTOR  FLASH_SECTOR_10   /* 备份扇区 */
#define FLASH_BACKUP_ADDR    0x080C0000u       /* 备份区起始地址 */
#define FLASH_BACKUP_SIZE    0x20000u          /* 备份区大小 128KB */

#define FLASH_RECORD_MAX     4096u             /* 单条记录最大字节数 */
#define FLASH_ROLLING_COUNT  32u               /* 滚存槽位数量 */

/* ========== 状态枚举 ========== */
typedef enum {
  FLASH_OK = 0,               /* 操作成功 */
  FLASH_ERR_ALIGN,            /* 地址未对齐 */
  FLASH_ERR_SIZE,             /* 数据过大 */
  FLASH_ERR_ERASE,            /* 擦除失败 */
  FLASH_ERR_PROGRAM,          /* 编程失败 */
  FLASH_ERR_CRC,              /* CRC 校验失败 */
  FLASH_ERR_BACKUP_RESTORE,   /* 已从备份恢复 */
  FLASH_ERR_TIMEOUT,          /* 操作超时 */
} Flash_Status_t;

/* ========== 滚存记录头（每条记录 12 字节头 + 数据） ========== */
typedef struct __attribute__((packed)) {
  uint32_t magic;             /* 魔术字 0x544F5301 (TOS\1) */
  uint32_t crc;               /* 数据 CRC32 */
  uint32_t datasize;          /* 用户数据字节数（读取步进用） */
  /* 后面紧跟 datasize 字节的用户数据 */
} Flash_Record_Header_t;

#define FLASH_RECORD_MAGIC    0x544F5301u
#define FLASH_HDR_SIZE        ((uint32_t)sizeof(Flash_Record_Header_t))

/* ========== API 函数 ========== */

/**
 * @brief  擦除数据存储扇区
 * @retval FLASH_OK / FLASH_ERR_ERASE
 */
Flash_Status_t Flash_Erase_Sector(void);

/**
 * @brief  写入数据到 Flash（覆盖写入，先擦除整个扇区）
 * @param  pData    源数据指针（须 32 位对齐）
 * @param  dataSize 数据字节数（最大 FLASH_RECORD_MAX，须 4 字节对齐）
 * @retval FLASH_OK / 错误码
 * @note   会短暂阻塞中断，对实时性有要求请在调用前关全局中断
 */
Flash_Status_t Flash_Write(const uint32_t *pData, uint32_t dataSize);

/**
 * @brief  从 Flash 读取数据
 * @param  pData    目标缓冲区
 * @param  dataSize 读取字节数
 * @retval FLASH_OK / 错误码
 */
Flash_Status_t Flash_Read(uint32_t *pData, uint32_t dataSize);

/**
 * @brief  带备份的安全写入（防掉电损坏）
 *         流程: 擦除备份区 → 备份旧数据 → 擦除数据区 → 写入新数据 → 清除备份
 *         中途掉电重启后自动从备份恢复
 * @param  pData    源数据指针
 * @param  dataSize 数据字节数
 * @retval FLASH_OK / FLASH_ERR_BACKUP_RESTORE / 其他错误
 */
Flash_Status_t Flash_Write_With_Backup(const uint32_t *pData, uint32_t dataSize);

/**
 * @brief  检查备份区是否有待恢复数据，有则自动恢复
 * @retval true 已恢复 / false 无需恢复
 * @note   应在系统启动时调用一次
 */
bool Flash_Check_Backup(void);

/**
 * @brief  滚存写入（在扇区内循环写入，延长 Flash 寿命）
 *         自动找到下一个空闲槽位写入，扇区写满后才擦除
 * @param  pData    源数据指针
 * @param  dataSize 数据字节数
 * @retval FLASH_OK / 错误码
 */
Flash_Status_t Flash_Rolling_Write(const uint32_t *pData, uint32_t dataSize);

/**
 * @brief  读取最后一次滚存写入的数据
 * @param  pData    目标缓冲区
 * @param  maxSize  缓冲区最大字节数
 * @param  outSize  输出实际数据字节数（可为 NULL）
 * @retval FLASH_OK / 错误码
 */
Flash_Status_t Flash_Rolling_Read(uint32_t *pData, uint32_t maxSize, uint32_t *outSize);

/**
 * @brief  计算 CRC32
 * @param  pData 数据指针
 * @param  size  字节数
 * @retval CRC32 值
 */
uint32_t Flash_CRC32(const uint32_t *pData, uint32_t size);

/**
 * @brief  调试打印 Flash 数据（十六进制）
 * @param  pData    数据指针
 * @param  dataSize 字节数
 */
void Flash_Print_Data(const uint32_t *pData, uint32_t dataSize);



/* ==================================================================
 * SD/FatFs helper API
 * ================================================================== */

#include "ff.h"

typedef void (*SFHD_SD_ProgressCallback)(const char *step, FRESULT result,
                                         void *user);

typedef struct {
  SFHD_SD_ProgressCallback progress;
  void *user;
} SFHD_SD_FormatOptions_t;

/**
 * @brief  Format SD card and create the TOS root filesystem layout.
 * @note   This helper uses FatFs f_mkfs() with explicit FM_FAT/FM_FAT32
 *         options. It first tries partitioned FAT, then retries SFD
 *         super-floppy layout. It also emits detailed logs through syslog.
 * @param  options Optional progress callback. Can be NULL.
 * @retval FatFs FRESULT.
 */
FRESULT SFHD_SD_FormatAndInit(const SFHD_SD_FormatOptions_t *options);

/**
 * @brief  Convert FatFs result to readable text for UI/logs.
 */
const char *SFHD_FResultName(FRESULT res);

/**
 * @brief  Convert FatFs result to system exception code.
 */
uint32_t SFHD_FResultToSysError(FRESULT res);

/**
 * @brief  Dump SD status and geometry to syslog.
 */
void SFHD_SD_DebugProbe(const char *tag);

#ifdef __cplusplus
}
#endif

#endif /* __SFHD_H */
