#include "hardware/include/flash_diskio.h"

#include "hardware/include/sfhd.h"
#include "tos_partitions.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define FLASHDISK_SECTOR_SIZE TOS_FLASH_FS_BLOCK_SIZE
#define FLASHDISK_META_SECTORS 4U
#define FLASHDISK_TMP_LUN  0U
#define FLASHDISK_DATA_LUN 1U

char TMPPath[4];
char DataPath[4];
FATFS TMPFatFS;
FATFS DataFatFS;

static uint8_t g_linked;
static uint32_t volume_base(BYTE lun) {
  return lun == FLASHDISK_TMP_LUN ? TOS_PART_TMP_ADDRESS
                                  : TOS_PART_USERDATA_ADDRESS;
}

static uint32_t volume_size(BYTE lun) {
  return lun == FLASHDISK_TMP_LUN ? TOS_PART_TMP_SIZE
                                  : TOS_PART_USERDATA_SIZE;
}

static uint32_t volume_sector_count(BYTE lun) {
  return volume_size(lun) / FLASHDISK_SECTOR_SIZE;
}

static uint8_t volume_valid(BYTE lun) {
  const uint8_t *b = (const uint8_t *)volume_base(lun);
  uint32_t sectors = volume_sector_count(lun);
  if (b[0] != 0xEBU || b[1] != 0x3CU || b[2] != 0x90U) return 0U;
  if (b[11] != (uint8_t)(FLASHDISK_SECTOR_SIZE & 0xFFU) ||
      b[12] != (uint8_t)((FLASHDISK_SECTOR_SIZE >> 8) & 0xFFU) ||
      b[13] != 0x01U) return 0U;
  if (b[14] != 0x01U || b[16] != 0x01U) return 0U;
  if (b[19] != (uint8_t)(sectors & 0xFFU) ||
      b[20] != (uint8_t)((sectors >> 8) & 0xFFU)) return 0U;
  return b[510] == 0x55U && b[511] == 0xAAU;
}

static void make_fat12_metadata(uint8_t *image, uint32_t sectors,
                                const char label[11]) {
  uint8_t *boot = image;
  uint8_t *fat = image + FLASHDISK_SECTOR_SIZE;
  memset(image, 0, FLASHDISK_META_SECTORS * FLASHDISK_SECTOR_SIZE);
  boot[0] = 0xEBU; boot[1] = 0x3CU; boot[2] = 0x90U;
  memcpy(&boot[3], "TOSFAT  ", 8U);
  boot[11] = (uint8_t)(FLASHDISK_SECTOR_SIZE & 0xFFU);
  boot[12] = (uint8_t)((FLASHDISK_SECTOR_SIZE >> 8) & 0xFFU);
  boot[13] = 0x01U;                  /* one sector/cluster */
  boot[14] = 0x01U; boot[15] = 0x00U;
  boot[16] = 0x01U;                  /* one FAT */
  boot[17] = 32U; boot[18] = 0U;     /* two root directory sectors */
  boot[19] = (uint8_t)(sectors & 0xFFU);
  boot[20] = (uint8_t)((sectors >> 8) & 0xFFU);
  boot[21] = 0xF8U;
  boot[22] = 0x01U; boot[23] = 0x00U; /* one FAT sector */
  boot[24] = 0x01U; boot[26] = 0x01U;
  boot[36] = 0x80U;
  boot[38] = 0x29U;
  boot[39] = 0x54U; boot[40] = 0x4FU; boot[41] = 0x53U; boot[42] = 0x31U;
  memcpy(&boot[43], label, 11U);
  memcpy(&boot[54], "FAT12   ", 8U);
  boot[510] = 0x55U; boot[511] = 0xAAU;
  fat[0] = 0xF8U; fat[1] = 0xFFU; fat[2] = 0xFFU;
}

static uint8_t flash_program_blob(uint32_t address, const uint8_t *data,
                                  uint32_t length) {
  HAL_StatusTypeDef hs;
  if ((address & 3U) != 0U || (length & 3U) != 0U) return 0U;
  if (HAL_FLASH_Unlock() != HAL_OK) return 0U;
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  for (uint32_t off = 0U; off < length; off += 4U) {
    uint32_t word;
    memcpy(&word, data + off, sizeof(word));
    hs = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + off, word);
    if (hs != HAL_OK) {
      (void)HAL_FLASH_Lock();
      return 0U;
    }
  }
  (void)HAL_FLASH_Lock();
  return memcmp((const void *)(uintptr_t)address, data, length) == 0 ? 1U : 0U;
}

static uint8_t erase_sector(uint32_t sector) {
  FLASH_EraseInitTypeDef erase;
  uint32_t error = 0U;
  if (HAL_FLASH_Unlock() != HAL_OK) return 0U;
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  erase.Sector = sector;
  erase.NbSectors = 1U;
  uint8_t ok = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK ? 1U : 0U;
  (void)HAL_FLASH_Lock();
  return ok;
}

static uint8_t build_volume(BYTE lun) {
  static uint8_t metadata[FLASHDISK_META_SECTORS * FLASHDISK_SECTOR_SIZE]
      __attribute__((aligned(4)));
  static const char tmp_label[11] = {'T','O','S',' ','T','M','P',' ',' ',' ',' '};
  static const char data_label[11] = {'T','O','S',' ','D','A','T','A',' ',' ',' '};
  make_fat12_metadata(metadata, volume_sector_count(lun),
                      lun == FLASHDISK_TMP_LUN ? tmp_label : data_label);
  return flash_program_blob(volume_base(lun), metadata, sizeof(metadata));
}

uint8_t FlashDiskIO_RebuildTmpAfterErase(void) {
  return build_volume(FLASHDISK_TMP_LUN);
}

uint8_t FlashDiskIO_RebuildUserdataAfterErase(void) {
  return build_volume(FLASHDISK_DATA_LUN);
}

uint8_t FlashDiskIO_LinkVolumes(void) {
  if (g_linked) return 1U;
  if (FATFS_LinkDriverEx(&TOS_FlashDisk_Driver, TMPPath,
                         FLASHDISK_TMP_LUN) != 0U) return 0U;
  if (FATFS_LinkDriverEx(&TOS_FlashDisk_Driver, DataPath,
                         FLASHDISK_DATA_LUN) != 0U) {
    (void)FATFS_UnLinkDriver(TMPPath);
    return 0U;
  }
  g_linked = 1U;
  return 1U;
}

uint8_t FlashDiskIO_EnsureVolumes(void) {
  if (!volume_valid(FLASHDISK_TMP_LUN)) {
    if (!erase_sector(FLASH_SECTOR_10) || !FlashDiskIO_RebuildTmpAfterErase()) {
      return 0U;
    }
  }

  if (!volume_valid(FLASHDISK_DATA_LUN)) {
    if (!erase_sector(FLASH_SECTOR_11) ||
        !FlashDiskIO_RebuildUserdataAfterErase()) {
      return 0U;
    }
  }
  return 1U;
}

static DSTATUS flashdisk_initialize(BYTE lun) {
  if (lun > FLASHDISK_DATA_LUN || !volume_valid(lun)) return STA_NOINIT;
  return STA_PROTECT;
}

static DSTATUS flashdisk_status(BYTE lun) {
  return flashdisk_initialize(lun);
}

static DRESULT flashdisk_read(BYTE lun, BYTE *buff, DWORD sector, UINT count) {
  uint32_t sectors;
  if (!buff || count == 0U || lun > FLASHDISK_DATA_LUN) return RES_PARERR;
  sectors = volume_sector_count(lun);
  if (sector >= sectors || count > sectors - sector) return RES_PARERR;
  memcpy(buff, (const void *)(volume_base(lun) + sector * FLASHDISK_SECTOR_SIZE),
         count * FLASHDISK_SECTOR_SIZE);
  return RES_OK;
}

#if _USE_WRITE == 1
static DRESULT flashdisk_write(BYTE lun, const BYTE *buff, DWORD sector,
                               UINT count) {
  (void)lun; (void)buff; (void)sector; (void)count;
  return RES_WRPRT;
}
#endif

#if _USE_IOCTL == 1
static DRESULT flashdisk_ioctl(BYTE lun, BYTE cmd, void *buff) {
  if (lun > FLASHDISK_DATA_LUN) return RES_PARERR;
  switch (cmd) {
    case CTRL_SYNC: return RES_OK;
    case GET_SECTOR_COUNT:
      if (!buff) return RES_PARERR;
      *(DWORD *)buff = volume_sector_count(lun);
      return RES_OK;
    case GET_SECTOR_SIZE:
      if (!buff) return RES_PARERR;
      *(WORD *)buff = FLASHDISK_SECTOR_SIZE;
      return RES_OK;
    case GET_BLOCK_SIZE:
      if (!buff) return RES_PARERR;
      *(DWORD *)buff = 1U;
      return RES_OK;
    default: return RES_PARERR;
  }
}
#endif

const Diskio_drvTypeDef TOS_FlashDisk_Driver = {
    flashdisk_initialize,
    flashdisk_status,
    flashdisk_read,
#if _USE_WRITE == 1
    flashdisk_write,
#endif
#if _USE_IOCTL == 1
    flashdisk_ioctl,
#endif
};
