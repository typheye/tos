#include "rec.h"

#include "rec_tdb.h"
#include "sbl_common.h"
#include "sbl_flash.h"
#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_state.h"

#define REC_CMD_ADDR 0x0801FC00UL
#define REC_CMD_MAGIC 0x52454331UL /* REC1 */
#define REC_TITLE_Y 88U
#define REC_STATUS_Y 112U
#define REC_DETAIL_Y 136U
#define REC_LINE_H 16U
#define REC_REBOOT_DELAY_MS 900U
#define REC_ERROR_DELAY_MS 1800U

typedef struct {
  uint32_t magic;
  uint32_t command;
  uint32_t arg;
  uint32_t crc;
} REC_Command;

static const char rec_title[] REC_CONST = "Recovery Mode";
static const char rec_tdb_ready[] REC_CONST = "TDB was ready";
static const char rec_tdb_hint[] REC_CONST = "Connect with computer";
static const char rec_tdb_fail[] REC_CONST = "TDB USB init failed";
static const char rec_format[] REC_CONST = "Formatting userdata...";
static const char rec_format_done[] REC_CONST = "Format completed";
static const char rec_format_fail[] REC_CONST = "Format failed";
static const char rec_clock_fail[] REC_CONST = "Clock init failed";
static const char rec_mounting[] REC_CONST = "Mounting SD...";
static const char rec_upgrade_fail[] REC_CONST = "Upgrade failed";
static const char rec_upgrade_done[] REC_CONST = "Upgrade completed";
static const char rec_init[] REC_CONST = "Initializing storage...";
static const char rec_init_fail[] REC_CONST = "Storage init failed";
static const char rec_init_done[] REC_CONST = "Storage initialized";

static REC_CODE uint16_t rec_text_width(const char *text) {
  uint16_t len = 0U;
  while (text[len] && text[len] != '\n') {
    len++;
  }
  return (uint16_t)(len * 6U);
}

static REC_CODE void rec_center(uint16_t y, const char *text, uint16_t color) {
  uint16_t w = rec_text_width(text);
  uint16_t x = (w >= SBL_LCD_W) ? 0U : (uint16_t)((SBL_LCD_W - w) / 2U);
  SBL_LcdDrawText(x, y, text, color, 1U);
}

static REC_CODE void rec_clear_line(uint16_t y) {
  SBL_LcdRect(0U, (uint16_t)(y - 2U), SBL_LCD_W, REC_LINE_H, SBL_BLACK);
}

static REC_CODE void rec_draw_status(const char *status, uint16_t color) {
  rec_clear_line(REC_STATUS_Y);
  rec_center(REC_STATUS_Y, status, color);
}

static REC_CODE void rec_draw_detail(const char *detail, uint16_t color) {
  rec_clear_line(REC_DETAIL_Y);
  if (detail && detail[0]) {
    rec_center(REC_DETAIL_Y, detail, color);
  }
}

static REC_CODE void rec_draw_full(const char *status, uint16_t color) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  rec_center(REC_TITLE_Y, rec_title, SBL_WHITE);
  rec_center(REC_STATUS_Y, status, color);
  SBL_LcdDisplayOn();
}

static REC_CODE uint32_t rec_crc(const REC_Command *cmd) {
  return cmd->magic ^ cmd->command ^ cmd->arg ^ 0xA55A5AA5UL;
}

static REC_CODE uint8_t rec_read_command(REC_Command *out) {
  const REC_Command *cmd = (const REC_Command *)REC_CMD_ADDR;
  if (!out || cmd->magic != REC_CMD_MAGIC || cmd->crc != rec_crc(cmd)) {
    return 0U;
  }
  *out = *cmd;
  return 1U;
}

static REC_CODE void rec_erase_command(void) {
  if (SBL_FlashUnlock()) {
    (void)SBL_FlashProgramWord(REC_CMD_ADDR, 0x00000000UL);
    SBL_FlashLock();
  }
}

static REC_CODE uint8_t rec_resolve_mode(uint8_t mode) {
  REC_Command cmd;
  uint32_t target = SBL_StateConsumeBootTarget();

  if (target == SBL_BOOT_TARGET_RECOVERY_FORMAT)
    return REC_MODE_FORMAT;
  if (target == SBL_BOOT_TARGET_RECOVERY_UPGRADE)
    return REC_MODE_UPGRADE;
  if (target == SBL_BOOT_TARGET_RECOVERY_INIT)
    return REC_MODE_INIT;
  if (target == SBL_BOOT_TARGET_RECOVERY)
    return REC_MODE_WAIT;

  if (mode != REC_MODE_WAIT && mode != REC_MODE_FORMAT &&
      mode != REC_MODE_UPGRADE && mode != REC_MODE_CLOCK_ERROR &&
      mode != REC_MODE_INIT)
    mode = REC_MODE_WAIT;

  /* Legacy command area is kept read-only compatible for already deployed
   * SYSTEM builds. New builds communicate exclusively through TEE state. */
  if (!rec_read_command(&cmd))
    return mode;
  rec_erase_command();
  if (mode != REC_MODE_WAIT)
    return mode;
  if (cmd.command == 1U)
    return REC_MODE_FORMAT;
  if (cmd.command == 2U)
    return REC_MODE_UPGRADE;
  if (cmd.command == 3U)
    return REC_MODE_INIT;
  return REC_MODE_WAIT;
}

static REC_CODE void rec_reboot_after_result(const char *status,
                                             uint16_t status_color,
                                             const char *detail) {
  rec_draw_status(status, status_color);
  rec_draw_detail(detail, detail ? SBL_RED : SBL_WHITE);
  SBL_DelayMs(detail ? REC_ERROR_DELAY_MS : REC_REBOOT_DELAY_MS);
  SBL_SystemReboot();
}

static REC_CODE uint8_t rec_upgrade_from_sd(void) {
  if (!REC_FatProbeInit()) {
    return 0U;
  }
  if (!REC_FatHasUpgradeManifest()) {
    return 0U;
  }
  return REC_FatFlashUpgrade(rec_draw_status);
}

REC_CODE void REC_Run(uint8_t mode) {
  uint8_t effective_mode;
  uint8_t ok;
  const char *error;

  SBL_LedsOff();
  SBL_LcdBacklightFull();
  effective_mode = rec_resolve_mode(mode);

  if (effective_mode == REC_MODE_WAIT) {
    /* CDC enumerates immediately and never waits for SD.  The SD/FatFs path is
     * opened only when a TDB filesystem command actually needs it, so an
     * absent or slow card cannot delay the COM device. */
    rec_draw_full(rec_tdb_ready, SBL_GREEN);
    rec_draw_detail(rec_tdb_hint, SBL_WHITE);
    SBL_DelayMs(20U);
    if (!REC_TDB_Start()) {
      rec_draw_status(rec_tdb_fail, SBL_RED);
    }
    while (1) {
      if (!REC_TDB_IsStarted()) {
        SBL_DelayMs(250U);
        (void)REC_TDB_Start();
      }
      REC_TDB_Tick();
      SBL_DelayMs(1U);
    }
  }

  if (effective_mode == REC_MODE_CLOCK_ERROR) {
    rec_draw_full(rec_clock_fail, SBL_RED);
    SBL_DelayMs(REC_ERROR_DELAY_MS);
    SBL_SystemReboot();
    return;
  }

  if (effective_mode == REC_MODE_FORMAT) {
    rec_draw_full(rec_format, SBL_WHITE);
    ok = REC_FatFormat();
    error = ok ? 0 : REC_FatLastError();
    rec_reboot_after_result(ok ? rec_format_done : rec_format_fail,
                            ok ? SBL_GREEN : SBL_RED, error);
    return;
  }

  if (effective_mode == REC_MODE_INIT) {
    rec_draw_full(rec_init, SBL_WHITE);
    ok = REC_FatInitStorage();
    error = ok ? 0 : REC_FatLastError();
    REC_FatRelease();
    rec_reboot_after_result(ok ? rec_init_done : rec_init_fail,
                            ok ? SBL_GREEN : SBL_RED, error);
    return;
  }

  rec_draw_full(rec_mounting, SBL_WHITE);
  ok = rec_upgrade_from_sd();
  error = ok ? 0 : REC_FatLastError();
  REC_FatRelease();
  rec_reboot_after_result(ok ? rec_upgrade_done : rec_upgrade_fail,
                          ok ? SBL_GREEN : SBL_RED, error);
}

void REC_Main(uint8_t clock_ok) {
  REC_Run(clock_ok ? REC_MODE_WAIT : REC_MODE_CLOCK_ERROR);
  while (1) {
    __NOP();
  }
}
