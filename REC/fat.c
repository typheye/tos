#include "rec.h"

#include "diskio.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "sbl_common.h"
#include "sbl_flash.h"
#include "sbl_mem.h"
#include "sdio.h"

#define REC_SD_TIMEOUT_MS 5000U
#define REC_FLASH_BUF_SZ  512U

static FATFS rec_fs;
static char rec_path[4];
static FIL rec_log_file;
static uint8_t rec_driver_linked;
static uint8_t rec_mounted;
static uint8_t rec_log_opened;
static uint32_t rec_card_blocks;
static uint32_t rec_card_block_size;

static uint8_t rec_flash_buf[REC_FLASH_BUF_SZ];
static uint32_t rec_scratch_words[128];

static SRE_CODE void rec_memcpy(uint8_t *dst, const uint8_t *src, uint32_t n) {
  while (n--) {
    *dst++ = *src++;
  }
}

static SRE_CODE uint8_t rec_wait_ready(uint32_t timeout_ms) {
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

static SRE_CODE DSTATUS rec_disk_initialize(BYTE lun) {
  HAL_SD_CardInfoTypeDef info;
  (void)lun;

  rec_card_blocks = 0U;
  rec_card_block_size = 512U;

  MX_SDIO_SD_Init();
  if (HAL_SD_Init(&hsd) != HAL_OK) {
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
    return STA_NOINIT;
  }

#ifdef SDIO_BUS_WIDE_4B
  (void)HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B);
#endif

  if (!rec_wait_ready(REC_SD_TIMEOUT_MS)) {
    return STA_NOINIT;
  }
  return 0U;
}

static SRE_CODE DSTATUS rec_disk_status(BYTE lun) {
  (void)lun;
  return rec_wait_ready(100U) ? 0U : STA_NOINIT;
}

static SRE_CODE DRESULT rec_disk_read(BYTE lun, BYTE *buff, DWORD sector,
                                      UINT count) {
  uint8_t *scratch = (uint8_t *)rec_scratch_words;
  (void)lun;
  if (!buff || count == 0U) {
    return RES_PARERR;
  }
  if (((uint32_t)buff & 3U) == 0U) {
    if (HAL_SD_ReadBlocks(&hsd, buff, sector, count, REC_SD_TIMEOUT_MS) != HAL_OK) {
      return RES_ERROR;
    }
    return rec_wait_ready(REC_SD_TIMEOUT_MS) ? RES_OK : RES_ERROR;
  }
  for (UINT i = 0U; i < count; ++i) {
    if (HAL_SD_ReadBlocks(&hsd, scratch, sector + i, 1U, REC_SD_TIMEOUT_MS) != HAL_OK) {
      return RES_ERROR;
    }
    if (!rec_wait_ready(REC_SD_TIMEOUT_MS)) {
      return RES_ERROR;
    }
    rec_memcpy(&buff[i * 512U], scratch, 512U);
  }
  return RES_OK;
}

static SRE_CODE DRESULT rec_disk_write(BYTE lun, const BYTE *buff, DWORD sector,
                                       UINT count) {
  uint8_t *scratch = (uint8_t *)rec_scratch_words;
  (void)lun;
  if (!buff || count == 0U) {
    return RES_PARERR;
  }
  if (((uint32_t)buff & 3U) == 0U) {
    if (HAL_SD_WriteBlocks(&hsd, (uint8_t *)buff, sector, count, REC_SD_TIMEOUT_MS) != HAL_OK) {
      return RES_ERROR;
    }
    return rec_wait_ready(REC_SD_TIMEOUT_MS) ? RES_OK : RES_ERROR;
  }
  for (UINT i = 0U; i < count; ++i) {
    rec_memcpy(scratch, &buff[i * 512U], 512U);
    if (HAL_SD_WriteBlocks(&hsd, scratch, sector + i, 1U, REC_SD_TIMEOUT_MS) != HAL_OK) {
      return RES_ERROR;
    }
    if (!rec_wait_ready(REC_SD_TIMEOUT_MS)) {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

static SRE_CODE DRESULT rec_disk_ioctl(BYTE lun, BYTE cmd, void *buff) {
  (void)lun;
  switch (cmd) {
    case CTRL_SYNC:
      return rec_wait_ready(REC_SD_TIMEOUT_MS) ? RES_OK : RES_ERROR;
    case GET_SECTOR_COUNT:
      *(DWORD *)buff = rec_card_blocks;
      return RES_OK;
    case GET_SECTOR_SIZE:
      *(WORD *)buff = 512U;
      return RES_OK;
    case GET_BLOCK_SIZE:
      *(DWORD *)buff = 1U;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}

static Diskio_drvTypeDef rec_sd_driver = {
    rec_disk_initialize,
    rec_disk_status,
    rec_disk_read,
    rec_disk_write,
    rec_disk_ioctl,
};

static SRE_CODE char *rec_append_char(char *p, char c) {
  *p++ = c;
  return p;
}

static SRE_CODE char *rec_append_str(char *p, const char *s) {
  while (*s) {
    *p++ = *s++;
  }
  return p;
}

static SRE_CODE char *rec_append_u32(char *p, uint32_t v) {
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

static SRE_CODE char *rec_append_u32_pad(char *p, uint32_t v,
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

static SRE_CODE void rec_make_log_path(char path[26], uint32_t index) {
  char *p = path;
  p = rec_append_str(p, "0:/system/rec/");
  p = rec_append_u32_pad(p, index, 8U, '0');
  p = rec_append_str(p, ".log");
  *p = 0;
}

static SRE_CODE void rec_log_line(const char *level, const char *tag,
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

static SRE_CODE void rec_log_status(const char *msg, uint16_t color) {
  (void)color;
  rec_log_line("INFO ", "REC  ", msg);
}

static SRE_CODE void rec_open_log(void) {
  char path[26];
  FILINFO info;

  if (rec_log_opened) {
    return;
  }
  (void)f_mkdir("0:/system");
  (void)f_mkdir("0:/system/rec");
  for (uint32_t i = 1U; i < 99999999UL; ++i) {
    rec_make_log_path(path, i);
    if (f_stat(path, &info) == FR_NO_FILE) {
      if (f_open(&rec_log_file, path, FA_CREATE_NEW | FA_WRITE) == FR_OK) {
        rec_log_opened = 1U;
        rec_log_line("INFO ", "REC  ", "Recovery log started");
      }
      return;
    }
  }
}

static SRE_CODE uint8_t rec_mount(void) {
  FRESULT fr;

  if (rec_mounted) {
    return 1U;
  }
  if (!rec_driver_linked) {
    if (FATFS_LinkDriver(&rec_sd_driver, rec_path) != 0U) {
      return 0U;
    }
    rec_driver_linked = 1U;
  }
  fr = f_mount(&rec_fs, rec_path, 1U);
  if (fr != FR_OK) {
    return 0U;
  }
  rec_mounted = 1U;
  rec_open_log();
  rec_log_line("INFO ", "SDIO ", "FatFs mounted");
  return 1U;
}

static SRE_CODE uint8_t rec_file_exists(const char *path) {
  FILINFO info;
  return (f_stat(path, &info) == FR_OK) ? 1U : 0U;
}

typedef struct {
  uint32_t crc;
} REC_CrcCtx;

static SRE_CODE uint8_t rec_crc_file(FIL *file, uint32_t *crc_out) {
  UINT br;
  REC_CrcCtx ctx;
  ctx.crc = SBL_FlashCrc32Seed();
  if (f_lseek(file, 0U) != FR_OK) {
    return 0U;
  }
  do {
    if (f_read(file, rec_flash_buf, sizeof(rec_flash_buf), &br) != FR_OK) {
      return 0U;
    }
    if (br > 0U) {
      ctx.crc = SBL_FlashCrc32Update(ctx.crc, rec_flash_buf, br);
    }
  } while (br > 0U);
  *crc_out = SBL_FlashCrc32Finish(ctx.crc);
  return 1U;
}

static SRE_CODE uint8_t rec_flash_file(const char *part_name,
                                       const char *path,
                                       void (*status)(const char *, uint16_t)) {
  FIL file;
  UINT br;
  uint32_t offset = 0U;
  uint32_t image_crc = 0U;
  const SBL_FlashPartition *part;
  SBL_FlashSession session;
  uint32_t image_size;

  if (f_open(&file, path, FA_READ) != FR_OK) {
    rec_log_line("INFO ", "REC  ", "Image not present, skipped");
    return 1U;
  }

  part = SBL_FlashFindPartition(part_name);
  image_size = (uint32_t)f_size(&file);
  if (!part || image_size != part->size) {
    (void)f_close(&file);
    rec_log_line("ERROR", "REC  ", "Bad image size");
    if (status) status("Bad image size", SBL_RED);
    return 0U;
  }

  if (status) status(path, SBL_WHITE);
  rec_log_line("INFO ", "REC  ", "Calculating image CRC");
  if (!rec_crc_file(&file, &image_crc)) {
    (void)f_close(&file);
    return 0U;
  }

  if (!SBL_FlashBegin(&session, part, image_size, image_crc)) {
    (void)f_close(&file);
    rec_log_line("ERROR", "FLASH", "Flash begin failed");
    return 0U;
  }

  if (f_lseek(&file, 0U) != FR_OK) {
    (void)f_close(&file);
    SBL_FlashAbort(&session);
    return 0U;
  }

  do {
    if (f_read(&file, rec_flash_buf, sizeof(rec_flash_buf), &br) != FR_OK) {
      (void)f_close(&file);
      SBL_FlashAbort(&session);
      return 0U;
    }
    if (br > 0U) {
      uint32_t chunk_crc = SBL_FlashCrc32Finish(
          SBL_FlashCrc32Update(SBL_FlashCrc32Seed(), rec_flash_buf, br));
      if ((br & 3U) != 0U ||
          !SBL_FlashWriteChunk(&session, offset, rec_flash_buf, br, chunk_crc)) {
        (void)f_close(&file);
        SBL_FlashAbort(&session);
        rec_log_line("ERROR", "FLASH", "Flash chunk failed");
        return 0U;
      }
      offset += br;
      if (status && ((offset & 0x3FFFU) == 0U || offset == image_size)) {
        status(path, SBL_WHITE);
      }
    }
  } while (br > 0U);

  (void)f_close(&file);
  if (!SBL_FlashFinalize(&session)) {
    rec_log_line("ERROR", "FLASH", "Flash finalize failed");
    return 0U;
  }
  rec_log_line("INFO ", "FLASH", "Image flashed");
  return 1U;
}

SRE_CODE uint8_t SRE_FatProbeInit(void) {
  rec_mounted = 0U;
  rec_log_opened = 0U;
  if (!rec_mount()) {
    return 0U;
  }
  if (!rec_file_exists("0:/init")) {
    rec_log_line("ERROR", "REC  ", "/init missing");
    return 0U;
  }
  rec_log_line("INFO ", "REC  ", "/init found");
  return 1U;
}

SRE_CODE uint8_t SRE_FatHasUpgradeManifest(void) {
  uint8_t ok = rec_file_exists("0:/data/upgrade/firmware/partitions.csv");
  rec_log_line(ok ? "INFO " : "ERROR", "REC  ",
               ok ? "Upgrade manifest found" : "Upgrade manifest missing");
  return ok;
}

SRE_CODE uint8_t SRE_FatFlashUpgrade(void (*status)(const char *, uint16_t)) {
  uint8_t wrote = 0U;

  if (!rec_mounted) {
    return 0U;
  }
  if (status) status("Flashing SAH...", SBL_WHITE);
  if (!rec_flash_file("sah", "0:/data/upgrade/firmware/sah.bin", status)) {
    return 0U;
  }
  wrote = 1U;

  if (status) status("Flashing SYSTEM...", SBL_WHITE);
  if (!rec_flash_file("system", "0:/data/upgrade/firmware/system.bin", status)) {
    return 0U;
  }
  wrote = 1U;

  rec_log_line("INFO ", "REC  ", "Upgrade completed");
  return wrote;
}
