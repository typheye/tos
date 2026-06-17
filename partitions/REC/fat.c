#include "rec.h"

#include "diskio.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "sbl_common.h"
#include "sbl_flash.h"
#include "sdio.h"
#include "tos_partitions.h"

#define REC_SD_TIMEOUT_MS 5000U
#define REC_FLASH_BUF_SZ  4096U
#define REC_SD_IO_ATTEMPTS 2U

/* Development safety switch: keep the REC format flow and bootloader
 * lock/unlock security semantics intact, but skip the destructive TMP and
 * USERDATA erase while this board is still being debugged. Set this to 0 for
 * production so lock/unlock performs the real data wipe required by policy. */
#ifndef REC_FORMAT_FAKE
#define REC_FORMAT_FAKE 1U
#endif

#define REC_TMP_SECTOR_INDEX 10U
#define REC_TMP_START        0x080C0000UL
#define REC_TMP_END          0x080E0000UL
#define REC_USERDATA_SECTOR_INDEX 11U
#define REC_USERDATA_START        0x080E0000UL
#define REC_USERDATA_END          0x08100000UL

static FATFS rec_fs;
static char rec_path[4];
static FIL rec_log_file;
static uint8_t rec_driver_linked;
static uint8_t rec_mounted;
static uint8_t rec_log_opened;
static uint32_t rec_card_blocks;
static uint32_t rec_card_block_size;
static char rec_last_error[64];

static uint8_t rec_flash_storage[REC_FLASH_BUF_SZ] __attribute__((aligned(4)));
static uint8_t rec_scratch_storage[512] __attribute__((aligned(4)));
static uint8_t *rec_flash_buf = rec_flash_storage;
static uint8_t *rec_scratch_buf = rec_scratch_storage;

static const char rec_err_none[] REC_CONST = "No error";
static const char rec_err_memory[] REC_CONST = "REC memory failed";
static const char rec_err_sdio[] REC_CONST = "SDIO init failed";
static const char rec_err_card_info[] REC_CONST = "Card info failed";
static const char rec_err_card_ready[] REC_CONST = "Card not ready";
static const char rec_err_sd_read[] REC_CONST = "SD read failed";
static const char rec_err_sd_write[] REC_CONST = "SD write failed";
static const char rec_err_link[] REC_CONST = "Disk driver link failed";
static const char rec_err_mount[] REC_CONST = "f_mount failed";
static const char rec_err_init_stat[] REC_CONST = "f_stat /init failed";
static const char rec_err_init_content[] REC_CONST = "Invalid /init marker";
static const char rec_err_manifest[] REC_CONST = "partitions.csv missing";
static const char rec_err_image_open[] REC_CONST = "Image open failed";
static const char rec_err_image_read[] REC_CONST = "Image read failed";
static const char rec_err_bad_size[] REC_CONST = "Bad image size";
static const char rec_err_flash_begin[] REC_CONST = "Flash begin failed";
static const char rec_err_flash_write[] REC_CONST = "Flash write failed";
static const char rec_err_flash_verify[] REC_CONST = "Flash verify failed";
static const char rec_err_format[] REC_CONST = "SD format failed";
static const char rec_err_format_verify[] REC_CONST = "SD verify failed";
static const char rec_err_layout[] REC_CONST = "Layout create failed";
static const char rec_err_userdata[] REC_CONST = "Userdata erase failed";
static const char rec_err_userdata_verify[] REC_CONST = "Userdata verify failed";
static const char rec_err_sync[] REC_CONST = "SD sync failed";
static const char rec_path_init[] REC_CONST = "0:/init";
static const char rec_path_manifest[] REC_CONST =
    "0:/storage/tos/upgrade/partitions.csv";
static const char rec_path_sbl[] REC_CONST =
    "0:/storage/tos/upgrade/firmware/sbl.bin";
static const char rec_path_tee[] REC_CONST =
    "0:/storage/tos/upgrade/firmware/tee.bin";
static const char rec_path_rec[] REC_CONST =
    "0:/storage/tos/upgrade/firmware/rec.bin";
static const char rec_path_sah[] REC_CONST =
    "0:/storage/tos/upgrade/firmware/sah.bin";
static const char rec_path_system[] REC_CONST =
    "0:/storage/tos/upgrade/firmware/system.bin";
static const char rec_status_sbl[] REC_CONST = "Flashing SBL...";
static const char rec_status_tee[] REC_CONST = "Flashing TEE...";
static const char rec_status_rec[] REC_CONST = "Flashing REC...";
static const char rec_status_sah[] REC_CONST = "Flashing SAH...";
static const char rec_status_system[] REC_CONST = "Flashing SYSTEM...";
static const char rec_log_started[] REC_CONST = "Recovery log started";
static const char rec_log_mounted[] REC_CONST = "FatFs mounted";
static const char rec_log_init_found[] REC_CONST = "/init found";
static const char rec_log_manifest_found[] REC_CONST = "Upgrade manifest found";
static const char rec_log_crc[] REC_CONST = "Calculating image CRC";
static const char rec_log_image_flashed[] REC_CONST = "Image flashed";
static const char rec_log_image_skipped[] REC_CONST = "Image not present, skipped";
static const char rec_log_upgrade_done[] REC_CONST = "Upgrade completed";
static const char rec_log_init_done[] REC_CONST = "Storage initialized";
static const char rec_err_no_images[] REC_CONST = "No upgrade images found";
static const char rec_err_too_many_staged[] REC_CONST = "Only one SBL/REC image allowed";

static REC_CODE void rec_copy_error(const char *msg) {
  uint32_t i = 0U;
  if (!msg) {
    msg = rec_err_none;
  }
  while (msg[i] && i < (uint32_t)(sizeof(rec_last_error) - 1U)) {
    rec_last_error[i] = msg[i];
    i++;
  }
  rec_last_error[i] = 0;
}

static REC_CODE const char *rec_fresult_name(FRESULT fr) {
  switch (fr) {
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

static REC_CODE void rec_set_error_fresult(const char *prefix, FRESULT fr) {
  char *p = rec_last_error;
  const char *name = rec_fresult_name(fr);
  uint32_t left = (uint32_t)(sizeof(rec_last_error) - 1U);

  while (prefix && *prefix && left) {
    *p++ = *prefix++;
    left--;
  }
  if (left >= 2U) {
    *p++ = ':';
    *p++ = ' ';
    left -= 2U;
  }
  while (*name && left) {
    *p++ = *name++;
    left--;
  }
  *p = 0;
}

static REC_CODE uint8_t rec_buffers_init(void) {
  return 1U;
}

static REC_CODE void rec_memcpy(uint8_t *dst, const uint8_t *src, uint32_t n) {
  while (n--) {
    *dst++ = *src++;
  }
}

static REC_CODE void rec_memset(uint8_t *dst, uint8_t value, uint32_t n) {
  while (n--) {
    *dst++ = value;
  }
}

static REC_CODE uint8_t rec_memeq(const uint8_t *a, const uint8_t *b,
                                  uint32_t n) {
  while (n--) {
    if (*a++ != *b++) {
      return 0U;
    }
  }
  return 1U;
}

static REC_CODE uint8_t rec_wait_ready(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();
  while ((uint32_t)(HAL_GetTick() - start) < timeout_ms) {
    HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd);
    if (state == HAL_SD_CARD_TRANSFER) {
      return 1U;
    }
    if (state == HAL_SD_CARD_ERROR) {
      return 0U;
    }
    SBL_DelayMs(1U);
  }
  return 0U;
}

static REC_CODE void rec_sd_drop(void) {
  if (hsd.Instance == SDIO) {
    /* REC uses polling I/O only.  Once a polling call returned, no DMA
     * transfer remains to abort; going straight to DeInit avoids issuing an
     * extra CMD13 to an already failing/removed card. */
    (void)HAL_SD_DeInit(&hsd);
  }
  rec_card_blocks = 0U;
  rec_card_block_size = 0U;
}

static REC_CODE DSTATUS rec_disk_initialize(BYTE lun) {
  HAL_SD_CardInfoTypeDef info;
  (void)lun;

  rec_card_blocks = 0U;
  rec_card_block_size = 512U;

  MX_SDIO_SD_Init();
  (void)HAL_SD_DeInit(&hsd);
  SBL_DelayMs(20U);
  MX_SDIO_SD_Init();
  __HAL_RCC_DMA2_CLK_ENABLE();
  if (HAL_SD_Init(&hsd) != HAL_OK) {
    rec_copy_error(rec_err_sdio);
    return STA_NOINIT;
  }
  SBL_DelayMs(200U);

  for (uint8_t i = 0U; i < 5U; ++i) {
    if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK && info.BlockNbr > 0U) {
      rec_card_blocks = info.BlockNbr;
      rec_card_block_size = info.BlockSize ? info.BlockSize : 512U;
      break;
    }
    SBL_DelayMs(100U);
  }
  if (rec_card_blocks == 0U) {
    rec_copy_error(rec_err_card_info);
    return STA_NOINIT;
  }

#ifdef SDIO_BUS_WIDE_4B
  (void)HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B);
#endif

  if (!rec_wait_ready(REC_SD_TIMEOUT_MS)) {
    rec_copy_error(rec_err_card_ready);
    return STA_NOINIT;
  }
  return 0U;
}

static REC_CODE DSTATUS rec_disk_status(BYTE lun) {
  (void)lun;
  if (rec_card_blocks == 0U) {
    return STA_NOINIT;
  }
  if (rec_wait_ready(100U)) {
    return 0U;
  }
  rec_sd_drop();
  return STA_NOINIT;
}

static REC_CODE DRESULT rec_disk_read(BYTE lun, BYTE *buff, DWORD sector,
                                      UINT count) {
  uint8_t *scratch = rec_scratch_buf;
  (void)lun;
  if (!buff || !scratch || count == 0U) {
    return RES_PARERR;
  }
  if (rec_card_blocks == 0U || sector >= rec_card_blocks ||
      count > (rec_card_blocks - sector)) {
    return RES_PARERR;
  }
  if (((uint32_t)buff & 3U) == 0U) {
    for (uint8_t attempt = 0U; attempt < REC_SD_IO_ATTEMPTS; ++attempt) {
      if (HAL_SD_ReadBlocks(&hsd, buff, sector, count,
                            REC_SD_TIMEOUT_MS) == HAL_OK &&
          rec_wait_ready(REC_SD_TIMEOUT_MS)) {
        return RES_OK;
      }
      rec_sd_drop();
      rec_copy_error(rec_err_sd_read);
      if (attempt + 1U < REC_SD_IO_ATTEMPTS &&
          rec_disk_initialize(0U) == 0U) {
        continue;
      }
      break;
    }
    return RES_ERROR;
  }
  for (UINT i = 0U; i < count; ++i) {
    uint8_t ok = 0U;
    for (uint8_t attempt = 0U; attempt < REC_SD_IO_ATTEMPTS; ++attempt) {
      if (HAL_SD_ReadBlocks(&hsd, scratch, sector + i, 1U,
                            REC_SD_TIMEOUT_MS) == HAL_OK &&
          rec_wait_ready(REC_SD_TIMEOUT_MS)) {
        ok = 1U;
        break;
      }
      rec_sd_drop();
      rec_copy_error(rec_err_sd_read);
      if (attempt + 1U < REC_SD_IO_ATTEMPTS &&
          rec_disk_initialize(0U) == 0U) {
        continue;
      }
      break;
    }
    if (!ok) {
      return RES_ERROR;
    }
    rec_memcpy(&buff[i * 512U], scratch, 512U);
  }
  return RES_OK;
}

static REC_CODE DRESULT rec_disk_write(BYTE lun, const BYTE *buff, DWORD sector,
                                       UINT count) {
  uint8_t *scratch = rec_scratch_buf;
  (void)lun;
  if (!buff || !scratch || count == 0U) {
    return RES_PARERR;
  }
  if (rec_card_blocks == 0U || sector >= rec_card_blocks ||
      count > (rec_card_blocks - sector)) {
    return RES_PARERR;
  }
  if (((uint32_t)buff & 3U) == 0U) {
    for (uint8_t attempt = 0U; attempt < REC_SD_IO_ATTEMPTS; ++attempt) {
      if (HAL_SD_WriteBlocks(&hsd, (uint8_t *)buff, sector, count,
                             REC_SD_TIMEOUT_MS) == HAL_OK &&
          rec_wait_ready(REC_SD_TIMEOUT_MS)) {
        return RES_OK;
      }
      rec_sd_drop();
      rec_copy_error(rec_err_sd_write);
      if (attempt + 1U < REC_SD_IO_ATTEMPTS &&
          rec_disk_initialize(0U) == 0U) {
        continue;
      }
      break;
    }
    return RES_ERROR;
  }
  for (UINT i = 0U; i < count; ++i) {
    rec_memcpy(scratch, &buff[i * 512U], 512U);
    uint8_t ok = 0U;
    for (uint8_t attempt = 0U; attempt < REC_SD_IO_ATTEMPTS; ++attempt) {
      if (HAL_SD_WriteBlocks(&hsd, scratch, sector + i, 1U,
                             REC_SD_TIMEOUT_MS) == HAL_OK &&
          rec_wait_ready(REC_SD_TIMEOUT_MS)) {
        ok = 1U;
        break;
      }
      rec_sd_drop();
      rec_copy_error(rec_err_sd_write);
      if (attempt + 1U < REC_SD_IO_ATTEMPTS &&
          rec_disk_initialize(0U) == 0U) {
        continue;
      }
      break;
    }
    if (!ok) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

static REC_CODE DRESULT rec_disk_ioctl(BYTE lun, BYTE cmd, void *buff) {
  (void)lun;
  switch (cmd) {
    case CTRL_SYNC:
      return rec_wait_ready(REC_SD_TIMEOUT_MS) ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
      if (!buff || rec_card_blocks == 0U) return RES_ERROR;
      *(DWORD *)buff = rec_card_blocks;
      return RES_OK;
    case GET_SECTOR_SIZE:
      if (!buff) return RES_PARERR;
      *(WORD *)buff = 512U;
      return RES_OK;
    case GET_BLOCK_SIZE:
      if (!buff) return RES_PARERR;
      *(DWORD *)buff = 1U;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}

static const Diskio_drvTypeDef rec_sd_driver REC_CONST = {
    rec_disk_initialize,
    rec_disk_status,
    rec_disk_read,
    rec_disk_write,
    rec_disk_ioctl,
};

static REC_CODE uint8_t rec_sd_write_preflight(void) {
  static const uint8_t signature[] REC_CONST = {'T', 'O', 'S', 'F', 'M', 'T'};

  rec_memset(rec_flash_buf, 0xA5U, REC_FLASH_BUF_SZ);
  rec_memcpy(rec_flash_buf, signature, (uint32_t)sizeof(signature));
  if (rec_disk_write(0U, rec_flash_buf, 0U, 1U) != RES_OK) {
    rec_copy_error(rec_err_sd_write);
    return 0U;
  }
  if (rec_disk_ioctl(0U, CTRL_SYNC, 0) != RES_OK) {
    rec_copy_error(rec_err_sync);
    return 0U;
  }

  rec_memset(rec_flash_buf, 0U, REC_FLASH_BUF_SZ);
  if (rec_disk_read(0U, rec_flash_buf, 0U, 1U) != RES_OK) {
    rec_copy_error(rec_err_sd_read);
    return 0U;
  }
  if (!rec_memeq(rec_flash_buf, signature, (uint32_t)sizeof(signature))) {
    rec_copy_error(rec_err_sd_write);
    return 0U;
  }
  return 1U;
}

static REC_CODE char *rec_append_char(char *p, char c) {
  *p++ = c;
  return p;
}

static REC_CODE char *rec_append_str(char *p, const char *s) {
  while (*s) {
    *p++ = *s++;
  }
  return p;
}

static REC_CODE char *rec_append_u32(char *p, uint32_t v) {
  char tmp[10];
  uint8_t n = 0U;
  if (v == 0U) {
    *p++ = '0';
    return p;
  }
  while (v && n < sizeof(tmp)) {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  }
  while (n) {
    *p++ = tmp[--n];
  }
  return p;
}

static REC_CODE char *rec_append_u32_pad(char *p, uint32_t v,
                                         uint8_t width, char pad) {
  char tmp[10];
  uint8_t n = 0U;
  do {
    tmp[n++] = (char)('0' + (v % 10U));
    v /= 10U;
  } while (v && n < sizeof(tmp));
  while (n < width) {
    *p++ = pad;
    width--;
  }
  while (n) {
    *p++ = tmp[--n];
  }
  return p;
}

static REC_CODE void rec_make_log_path(char path[40], uint32_t index) {
  char *p = path;
  p = rec_append_str(p, "0:/storage/tos/rec/");
  p = rec_append_u32_pad(p, index, 8U, '0');
  p = rec_append_str(p, ".rec");
  *p = 0;
}

static REC_CODE void rec_log_line(const char *level, const char *tag,
                                  const char *msg) {
  char line[128];
  char *p;
  UINT bw;
  uint32_t tick;

  if (!rec_log_opened) {
    return;
  }
  tick = HAL_GetTick();
  p = line;
  p = rec_append_str(p, "[");
  p = rec_append_u32_pad(p, tick / 1000U, 5U, ' ');
  p = rec_append_char(p, '.');
  p = rec_append_u32_pad(p, tick % 1000U, 3U, '0');
  p = rec_append_str(p, "] [");
  p = rec_append_str(p, level);
  p = rec_append_str(p, "] [");
  p = rec_append_str(p, tag);
  p = rec_append_str(p, "] ");
  p = rec_append_str(p, msg);
  p = rec_append_str(p, "\r\n");
  (void)f_write(&rec_log_file, line, (UINT)(p - line), &bw);
  (void)f_sync(&rec_log_file);
}

static REC_CODE void rec_log_status(const char *msg, uint16_t color) {
  (void)color;
  rec_log_line("INFO ", "REC  ", msg);
}

static REC_CODE void rec_open_log(void) {
  char path[40];
  FILINFO info;

  if (rec_log_opened) {
    return;
  }
  (void)f_mkdir("0:/storage");
  (void)f_mkdir("0:/storage/tos");
  (void)f_mkdir("0:/storage/tos/rec");
  for (uint32_t i = 1U; i < 99999999UL; ++i) {
    rec_make_log_path(path, i);
    if (f_stat(path, &info) == FR_NO_FILE) {
      if (f_open(&rec_log_file, path, FA_CREATE_NEW | FA_WRITE) == FR_OK) {
        rec_log_opened = 1U;
        rec_log_line("INFO ", "REC  ", rec_log_started);
      }
      return;
    }
  }
}

static REC_CODE uint8_t rec_mount(void) {
  FRESULT fr;

  if (rec_mounted) {
    return 1U;
  }
  if (!rec_driver_linked) {
    if (FATFS_LinkDriver(&rec_sd_driver, rec_path) != 0U) {
      rec_copy_error(rec_err_link);
      return 0U;
    }
    rec_driver_linked = 1U;
  }
  fr = f_mount(&rec_fs, rec_path, 1U);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_mount, fr);
    return 0U;
  }
  rec_mounted = 1U;
  rec_open_log();
  rec_log_line("INFO ", "SDIO ", rec_log_mounted);
  return 1U;
}

static REC_CODE uint8_t rec_file_exists(const char *path, FRESULT *out_fr) {
  FILINFO info;
  FRESULT fr = f_stat(path, &info);
  if (out_fr) {
    *out_fr = fr;
  }
  return (fr == FR_OK) ? 1U : 0U;
}

typedef struct {
  uint32_t crc;
} REC_CrcCtx;

static REC_CODE uint8_t rec_crc_file(FIL *file, uint32_t *crc_out) {
  UINT br;
  FRESULT fr;
  REC_CrcCtx ctx;
  ctx.crc = SBL_FlashCrc32Seed();
  fr = f_lseek(file, 0U);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_image_read, fr);
    return 0U;
  }
  do {
    fr = f_read(file, rec_flash_buf, REC_FLASH_BUF_SZ, &br);
    if (fr != FR_OK) {
      rec_set_error_fresult(rec_err_image_read, fr);
      return 0U;
    }
    if (br > 0U) {
      ctx.crc = SBL_FlashCrc32Update(ctx.crc, rec_flash_buf, br);
    }
  } while (br > 0U);
  *crc_out = SBL_FlashCrc32Finish(ctx.crc);
  return 1U;
}

static REC_CODE uint8_t rec_flash_file(const char *part_name,
                                       const char *path,
                                       const char *status_text,
                                       void (*status)(const char *, uint16_t),
                                       uint8_t *flashed) {
  FIL file;
  UINT br;
  FRESULT fr;
  uint32_t offset = 0U;
  uint32_t image_crc = 0U;
  const SBL_FlashPartition *part;
  SBL_FlashSession session;
  uint32_t image_size;

  fr = f_open(&file, path, FA_READ);
  if (fr != FR_OK) {
    if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
      rec_log_line("INFO ", "REC  ", rec_log_image_skipped);
      return 1U;
    }
    rec_set_error_fresult(rec_err_image_open, fr);
    rec_log_line("ERROR", "REC  ", REC_FatLastError());
    return 0U;
  }

  part = SBL_FlashFindPartition(part_name);
  image_size = (uint32_t)f_size(&file);
  if (!part || image_size != part->size) {
    (void)f_close(&file);
    rec_copy_error(rec_err_bad_size);
    rec_log_line("ERROR", "REC  ", rec_err_bad_size);
    if (status) status(rec_err_bad_size, SBL_RED);
    return 0U;
  }

  if (status) status(status_text, SBL_WHITE);
  rec_log_line("INFO ", "REC  ", rec_log_crc);
  if (!rec_crc_file(&file, &image_crc)) {
    (void)f_close(&file);
    return 0U;
  }

  if (!SBL_FlashBegin(&session, part, image_size, image_crc)) {
    (void)f_close(&file);
    rec_copy_error(rec_err_flash_begin);
    rec_log_line("ERROR", "FLASH", rec_err_flash_begin);
    return 0U;
  }

  fr = f_lseek(&file, 0U);
  if (fr != FR_OK) {
    (void)f_close(&file);
    SBL_FlashAbort(&session);
    rec_set_error_fresult(rec_err_image_read, fr);
    return 0U;
  }

  do {
    fr = f_read(&file, rec_flash_buf, REC_FLASH_BUF_SZ, &br);
    if (fr != FR_OK) {
      (void)f_close(&file);
      SBL_FlashAbort(&session);
      rec_set_error_fresult(rec_err_image_read, fr);
      return 0U;
    }
    if (br > 0U) {
      uint32_t chunk_crc = SBL_FlashCrc32Finish(
          SBL_FlashCrc32Update(SBL_FlashCrc32Seed(), rec_flash_buf, br));
      if ((br & 3U) != 0U ||
          !SBL_FlashWriteChunk(&session, offset, rec_flash_buf, br, chunk_crc)) {
        (void)f_close(&file);
        SBL_FlashAbort(&session);
        rec_copy_error(rec_err_flash_write);
        rec_log_line("ERROR", "FLASH", rec_err_flash_write);
        return 0U;
      }
      offset += br;
      if (status && ((offset & 0x3FFFU) == 0U || offset == image_size)) {
        status(status_text, SBL_WHITE);
      }
    }
  } while (br > 0U);

  (void)f_close(&file);
  if (!SBL_FlashFinalize(&session)) {
    rec_copy_error(rec_err_flash_verify);
    rec_log_line("ERROR", "FLASH", rec_err_flash_verify);
    return 0U;
  }
  rec_log_line("INFO ", "FLASH", rec_log_image_flashed);
  if (flashed) {
    *flashed = 1U;
  }
  return 1U;
}

REC_CODE uint8_t REC_FatProbeInit(void) {
  FRESULT fr;
  FIL file;
  FILINFO info;
  UINT br = 0U;
  rec_copy_error(rec_err_none);
  rec_mounted = 0U;
  rec_log_opened = 0U;
  if (!rec_buffers_init()) {
    return 0U;
  }
  if (!rec_mount()) {
    return 0U;
  }
  fr = f_stat(rec_path_init, &info);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_init_stat, fr);
    rec_log_line("ERROR", "REC  ", REC_FatLastError());
    return 0U;
  }
  if ((info.fattrib & AM_DIR) != 0U || info.fsize != TOS_SD_INIT_FILE_SIZE) {
    rec_copy_error(rec_err_init_content);
    rec_log_line("ERROR", "REC  ", REC_FatLastError());
    return 0U;
  }

  fr = f_open(&file, rec_path_init, FA_READ);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_init_stat, fr);
    rec_log_line("ERROR", "REC  ", REC_FatLastError());
    return 0U;
  }
  for (uint32_t off = 0U; off < TOS_SD_INIT_FILE_SIZE;
       off += REC_FLASH_BUF_SZ) {
    fr = f_read(&file, rec_flash_buf, REC_FLASH_BUF_SZ, &br);
    if (fr != FR_OK || br != REC_FLASH_BUF_SZ) {
      (void)f_close(&file);
      rec_set_error_fresult(rec_err_init_stat,
                            fr == FR_OK ? FR_DISK_ERR : fr);
      rec_log_line("ERROR", "REC  ", REC_FatLastError());
      return 0U;
    }
    for (uint32_t i = 0U; i < REC_FLASH_BUF_SZ; ++i) {
      if (rec_flash_buf[i] != 0xFFU) {
        (void)f_close(&file);
        rec_copy_error(rec_err_init_content);
        rec_log_line("ERROR", "REC  ", REC_FatLastError());
        return 0U;
      }
    }
  }
  if (f_close(&file) != FR_OK) {
    rec_copy_error(rec_err_init_content);
    rec_log_line("ERROR", "REC  ", REC_FatLastError());
    return 0U;
  }
  rec_log_line("INFO ", "REC  ", rec_log_init_found);
  return 1U;
}

REC_CODE uint8_t REC_FatHasUpgradeManifest(void) {
  FRESULT fr;
  uint8_t ok = rec_file_exists(rec_path_manifest, &fr);
  if (!ok) {
    if (fr == FR_NO_FILE || fr == FR_NO_PATH) {
      rec_copy_error(rec_err_manifest);
    } else {
      rec_set_error_fresult(rec_err_manifest, fr);
    }
  }
  rec_log_line(ok ? "INFO " : "ERROR", "REC  ",
               ok ? rec_log_manifest_found : REC_FatLastError());
  return ok;
}

REC_CODE uint8_t REC_FatFlashUpgrade(void (*status)(const char *, uint16_t)) {
  uint8_t wrote = 0U;
  uint8_t staged_count = 0U;
  FRESULT fr;

  if (!rec_mounted) {
    rec_copy_error(rec_err_mount);
    return 0U;
  }
  if (rec_file_exists(rec_path_sbl, &fr)) staged_count++;
  if (rec_file_exists(rec_path_rec, &fr)) staged_count++;
  if (staged_count > 1U) {
    rec_copy_error(rec_err_too_many_staged);
    rec_log_line("ERROR", "REC  ", rec_err_too_many_staged);
    return 0U;
  }

  if (status) status(rec_status_tee, SBL_WHITE);
  if (!rec_flash_file("tee", rec_path_tee, rec_status_tee, status, &wrote)) {
    return 0U;
  }

  if (status) status(rec_status_sah, SBL_WHITE);
  if (!rec_flash_file("sah", rec_path_sah, rec_status_sah, status, &wrote)) {
    return 0U;
  }

  if (status) status(rec_status_system, SBL_WHITE);
  if (!rec_flash_file("system", rec_path_system, rec_status_system, status, &wrote)) {
    return 0U;
  }

  if (status) status(rec_status_rec, SBL_WHITE);
  if (!rec_flash_file("rec", rec_path_rec, rec_status_rec, status, &wrote)) {
    return 0U;
  }

  if (status) status(rec_status_sbl, SBL_WHITE);
  if (!rec_flash_file("sbl", rec_path_sbl, rec_status_sbl, status, &wrote)) {
    return 0U;
  }

  if (!wrote) {
    rec_copy_error(rec_err_no_images);
    rec_log_line("ERROR", "REC  ", rec_err_no_images);
    return 0U;
  }
  rec_log_line("INFO ", "REC  ", rec_log_upgrade_done);
  return wrote;
}

static REC_CODE uint8_t rec_mkdir_ok(const char *path) {
  FRESULT fr = f_mkdir(path);
  if (fr == FR_OK || fr == FR_EXIST) {
    return 1U;
  }
  rec_set_error_fresult(rec_err_layout, fr);
  return 0U;
}

static REC_CODE uint8_t rec_write_init_marker(void) {
  FIL file;
  FILINFO info;
  UINT bw = 0U;
  FRESULT fr = f_open(&file, rec_path_init, FA_CREATE_ALWAYS | FA_WRITE);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_layout, fr);
    return 0U;
  }

  rec_memset(rec_flash_buf, 0xFFU, REC_FLASH_BUF_SZ);
  for (uint32_t off = 0U; off < TOS_SD_INIT_FILE_SIZE && fr == FR_OK;
       off += REC_FLASH_BUF_SZ) {
    fr = f_write(&file, rec_flash_buf, REC_FLASH_BUF_SZ, &bw);
    if (fr == FR_OK && bw != REC_FLASH_BUF_SZ) fr = FR_DISK_ERR;
  }
  if (fr == FR_OK) fr = f_sync(&file);

  {
    FRESULT close_fr = f_close(&file);
    if (fr == FR_OK) {
      fr = close_fr;
    }
  }
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_layout, fr);
    return 0U;
  }

  fr = f_stat(rec_path_init, &info);
  if (fr != FR_OK || (info.fattrib & AM_DIR) != 0U ||
      info.fsize != TOS_SD_INIT_FILE_SIZE) {
    rec_set_error_fresult(rec_err_format_verify,
                          fr == FR_OK ? FR_INVALID_OBJECT : fr);
    return 0U;
  }
  return 1U;
}

static REC_CODE uint8_t rec_create_layout(void) {
  static const char *const dirs[] REC_CONST = {
      "0:/storage",
      "0:/storage/tos",
      "0:/storage/tos/log",
      "0:/storage/tos/dump",
      "0:/storage/tos/rec",
      "0:/storage/tos/upgrade",
      "0:/storage/tos/upgrade/firmware",
  };

  for (uint32_t i = 0U; i < (uint32_t)(sizeof(dirs) / sizeof(dirs[0])); ++i) {
    if (!rec_mkdir_ok(dirs[i])) {
      return 0U;
    }
  }
  return rec_write_init_marker();
}

REC_CODE uint8_t REC_FatInitStorage(void) {
  FRESULT fr;
  DWORD free_clusters = 0U;
  FATFS *mounted_fs = 0;

  REC_FatRelease();
  rec_copy_error(rec_err_none);
  if (!rec_buffers_init()) {
    return 0U;
  }
  if (FATFS_LinkDriver(&rec_sd_driver, rec_path) != 0U) {
    rec_copy_error(rec_err_link);
    return 0U;
  }
  rec_driver_linked = 1U;

  if (rec_disk_initialize(0U) != 0U) {
    return 0U;
  }
  if (!rec_sd_write_preflight()) {
    return 0U;
  }
  fr = f_mkfs(rec_path, (BYTE)(FM_FAT | FM_FAT32), 0U,
              rec_flash_buf, REC_FLASH_BUF_SZ);
  if (fr != FR_OK) {
    rec_sd_drop();
    if (rec_disk_initialize(0U) != 0U) {
      return 0U;
    }
    fr = f_mkfs(rec_path, (BYTE)(FM_FAT | FM_FAT32 | FM_SFD), 0U,
                rec_flash_buf, REC_FLASH_BUF_SZ);
  }
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_format, fr);
    return 0U;
  }
  if (rec_disk_ioctl(0U, CTRL_SYNC, 0) != RES_OK) {
    rec_copy_error(rec_err_sync);
    return 0U;
  }

  fr = f_mount(&rec_fs, rec_path, 1U);
  if (fr != FR_OK) {
    rec_set_error_fresult(rec_err_format_verify, fr);
    return 0U;
  }
  rec_mounted = 1U;

  fr = f_getfree(rec_path, &free_clusters, &mounted_fs);
  if (fr != FR_OK || !mounted_fs) {
    rec_set_error_fresult(rec_err_format_verify,
                          fr == FR_OK ? FR_INVALID_OBJECT : fr);
    return 0U;
  }
  if (!rec_create_layout()) {
    return 0U;
  }
  if (rec_disk_ioctl(0U, CTRL_SYNC, 0) != RES_OK) {
    rec_copy_error(rec_err_sync);
    return 0U;
  }

  rec_open_log();
  rec_log_line("INFO ", "REC  ", rec_log_init_done);
  return 1U;
}

REC_CODE uint8_t REC_FatFormat(void) {
  rec_copy_error(rec_err_none);

#if REC_FORMAT_FAKE
  return 1U;
#else
  if (!SBL_FlashUnlock()) {
    rec_copy_error(rec_err_userdata);
    return 0U;
  }
  if (!SBL_FlashEraseSectorIndex(REC_TMP_SECTOR_INDEX)) {
    SBL_FlashLock();
    rec_copy_error(rec_err_userdata);
    return 0U;
  }
  if (!SBL_FlashEraseSectorIndex(REC_USERDATA_SECTOR_INDEX)) {
    SBL_FlashLock();
    rec_copy_error(rec_err_userdata);
    return 0U;
  }
  SBL_FlashLock();
  SBL_FlashFlushCaches();

  for (uint32_t addr = REC_TMP_START; addr < REC_TMP_END;
       addr += sizeof(uint32_t)) {
    if (*(const volatile uint32_t *)addr != 0xFFFFFFFFUL) {
      rec_copy_error(rec_err_userdata_verify);
      return 0U;
    }
  }
  for (uint32_t addr = REC_USERDATA_START; addr < REC_USERDATA_END;
       addr += sizeof(uint32_t)) {
    if (*(const volatile uint32_t *)addr != 0xFFFFFFFFUL) {
      rec_copy_error(rec_err_userdata_verify);
      return 0U;
    }
  }
  return 1U;
#endif
}

REC_CODE void REC_FatRelease(void) {
  if (rec_log_opened) {
    (void)f_sync(&rec_log_file);
    (void)f_close(&rec_log_file);
    rec_log_opened = 0U;
  }

  if (rec_mounted) {
    (void)f_mount(0, rec_path, 0U);
    rec_mounted = 0U;
  }

  if (rec_driver_linked) {
    (void)FATFS_UnLinkDriver(rec_path);
    rec_driver_linked = 0U;
  }

  if (hsd.Instance == SDIO) {
    (void)HAL_SD_DeInit(&hsd);
  }
  rec_card_blocks = 0U;
  rec_card_block_size = 0U;
}

REC_CODE const char *REC_FatLastError(void) {
  return rec_last_error[0] ? rec_last_error : rec_err_none;
}
