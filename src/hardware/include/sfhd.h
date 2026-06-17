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
#include "core/sys/include/syslog.h"
#include <stdio.h>
#include <string.h>
#include "diskio.h"
#include "fatfs.h"
#include "ff.h"
#include "core/include/syshandle.h"

#ifdef __cplusplus
extern "C" {
#endif


#define FLASH_DATA_SECTOR    FLASH_SECTOR_11   
#define FLASH_DATA_ADDR      0x080E0000u       
#define FLASH_DATA_SIZE      0x1FC00u          
#define FLASH_BL_STATE_ADDR  0x0800FC00u
#define FLASH_BL_STATE_SIZE  0x400u

#define FLASH_BL_BOOT_NONE     0xFFFFFFFFu
#define FLASH_BL_BOOT_FASTBOOT 0x46424F54u
#define FLASH_BL_BOOT_RECOVERY 0x52454356u
#define FLASH_BL_BOOT_RECOVERY_UPGRADE 0x52555047u

#define FLASH_BACKUP_SECTOR  FLASH_SECTOR_10   
#define FLASH_BACKUP_ADDR    0x080C0000u       
#define FLASH_BACKUP_SIZE    0x20000u          

#define FLASH_RECORD_MAX     4096u             
#define FLASH_ROLLING_COUNT  32u               


typedef enum {
  FLASH_OK = 0,               
  FLASH_ERR_ALIGN,            
  FLASH_ERR_SIZE,             
  FLASH_ERR_ERASE,            
  FLASH_ERR_PROGRAM,          
  FLASH_ERR_CRC,              
  FLASH_ERR_BACKUP_RESTORE,   
  FLASH_ERR_TIMEOUT,          
} Flash_Status_t;


typedef struct __attribute__((packed)) {
  uint32_t magic;             
  uint32_t crc;               
  uint32_t datasize;          
  
} Flash_Record_Header_t;

#define FLASH_RECORD_MAGIC    0x544F5301u
#define FLASH_HDR_SIZE        ((uint32_t)sizeof(Flash_Record_Header_t))




Flash_Status_t Flash_Erase_Sector(void);


Flash_Status_t Flash_Write(const uint32_t *pData, uint32_t dataSize);


Flash_Status_t Flash_Read(uint32_t *pData, uint32_t dataSize);


Flash_Status_t Flash_Write_With_Backup(const uint32_t *pData, uint32_t dataSize);


bool Flash_Check_Backup(void);


Flash_Status_t Flash_Rolling_Write(const uint32_t *pData, uint32_t dataSize);


Flash_Status_t Flash_Rolling_Read(uint32_t *pData, uint32_t maxSize, uint32_t *outSize);


uint32_t Flash_CRC32(const uint32_t *pData, uint32_t size);


void Flash_Print_Data(const uint32_t *pData, uint32_t dataSize);

Flash_Status_t Flash_BL_SetBootTarget(uint32_t target);



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
