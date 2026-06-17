#include "rec.h"

#include "sbl_common.h"
#include "sbl_flash.h"
#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "rec_msc.h"

#define REC_CMD_ADDR       0x0801FC00UL
#define REC_CMD_MAGIC      0x52454331UL /* REC1 */
#define REC_TITLE_Y        88U
#define REC_STATUS_Y       112U
#define REC_DETAIL_Y       136U
#define REC_USB_Y          156U
#define REC_LINE_H         16U

extern uint32_t HAL_GetTick(void);

typedef struct {
  uint32_t magic;
  uint32_t command;
  uint32_t arg;
  uint32_t crc;
} REC_Command;

static const char rec_title[] REC_CONST = "Recovery Mode";
static const char rec_restart[] REC_CONST = "Press the RST button to restart.";
static const char rec_format[] REC_CONST = "Formatting userdata...";
static const char rec_format_done[] REC_CONST = "Format completed";
static const char rec_clock_fail[] REC_CONST = "Clock init failed";
static const char rec_mounting[] REC_CONST = "Mounting SD...";
static const char rec_upgrade_fail[] REC_CONST = "Upgrade failed";
static const char rec_upgrade_done[] REC_CONST = "Upgrade completed";
static const char rec_usb_connecting[] REC_CONST = "Connecting USB storage...";
static const char rec_usb_ready[] REC_CONST = "USB storage ready";

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

static REC_CODE void rec_draw_usb(const char *status, uint16_t color) {
  rec_clear_line(REC_USB_Y);
  rec_center(REC_USB_Y, status, color);
}

static REC_CODE void rec_draw_full(const char *status, uint16_t color) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  rec_center(REC_TITLE_Y, rec_title, SBL_WHITE);
  rec_center(REC_STATUS_Y, status, color);
  SBL_LcdDisplayOn();
}

static REC_CODE void rec_draw_terminal(const char *detail, uint16_t color) {
  rec_draw_status(rec_restart, SBL_GREEN);
  rec_draw_detail(detail, color);
  rec_draw_usb(rec_usb_connecting, SBL_WHITE);
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

  if (mode != REC_MODE_WAIT && mode != REC_MODE_FORMAT &&
      mode != REC_MODE_UPGRADE && mode != REC_MODE_CLOCK_ERROR) {
    mode = REC_MODE_WAIT;
  }
  if (!rec_read_command(&cmd)) {
    return mode;
  }

  rec_erase_command();
  if (mode != REC_MODE_WAIT) {
    return mode;
  }
  if (cmd.command == 1U) {
    return REC_MODE_FORMAT;
  }
  if (cmd.command == 2U) {
    return REC_MODE_UPGRADE;
  }
  return REC_MODE_WAIT;
}

static REC_CODE uint8_t rec_format_userdata(void) {
  REC_MSC_Stop();
  REC_FatRelease();
  rec_draw_status(rec_format, SBL_RED);
  return REC_FatFormat();
}

static REC_CODE uint8_t rec_upgrade_from_sd(const char **detail,
                                            uint16_t *detail_color) {
  uint8_t ok = 0U;

  REC_MSC_Stop();
  rec_draw_status(rec_mounting, SBL_WHITE);
  if (!REC_FatProbeInit()) {
    *detail = REC_FatLastError();
    *detail_color = SBL_RED;
    goto done;
  }
  if (!REC_FatHasUpgradeManifest()) {
    *detail = REC_FatLastError();
    *detail_color = SBL_RED;
    goto done;
  }
  if (!REC_FatFlashUpgrade(rec_draw_status)) {
    const char *error = REC_FatLastError();
    *detail = (error && error[0]) ? error : rec_upgrade_fail;
    *detail_color = SBL_RED;
    goto done;
  }

  *detail = rec_upgrade_done;
  *detail_color = SBL_GREEN;
  ok = 1U;

done:
  REC_FatRelease();
  return ok;
}

REC_CODE void REC_Run(uint8_t mode) {
  const char *detail = 0;
  uint16_t detail_color = SBL_WHITE;
  uint8_t effective_mode;
  uint8_t usb_was_ready = 0U;

  SBL_LedsOff();
  SBL_LcdBacklightFull();
  effective_mode = rec_resolve_mode(mode);

  if (effective_mode == REC_MODE_CLOCK_ERROR) {
    rec_draw_full(rec_restart, SBL_GREEN);
    rec_draw_detail(rec_clock_fail, SBL_RED);
  } else if (effective_mode == REC_MODE_WAIT) {
    rec_draw_full(rec_restart, SBL_GREEN);
  } else {
    rec_draw_full(effective_mode == REC_MODE_FORMAT ? rec_format : rec_mounting,
                  SBL_WHITE);
  }

  if (effective_mode == REC_MODE_FORMAT) {
    if (rec_format_userdata()) {
      detail = rec_format_done;
      detail_color = SBL_GREEN;
    } else {
      detail = REC_FatLastError();
      detail_color = SBL_RED;
    }
    REC_FatRelease();
    rec_draw_terminal(detail, detail_color);
  } else if (effective_mode == REC_MODE_UPGRADE) {
    (void)rec_upgrade_from_sd(&detail, &detail_color);
    rec_draw_terminal(detail, detail_color);
  } else if (effective_mode != REC_MODE_CLOCK_ERROR) {
    rec_draw_usb(rec_usb_connecting, SBL_WHITE);
  }

  if (effective_mode != REC_MODE_CLOCK_ERROR) {
    (void)REC_MSC_Start();
  }

  while (1) {
    if (effective_mode != REC_MODE_CLOCK_ERROR) {
      REC_MSC_Tick();
      if (REC_MSC_IsConfigured()) {
        if (!usb_was_ready) {
          rec_draw_usb(rec_usb_ready, SBL_GREEN);
          usb_was_ready = 1U;
        }
      } else if (usb_was_ready || !REC_MSC_IsStarted()) {
        rec_draw_usb(rec_usb_connecting, SBL_WHITE);
        usb_was_ready = 0U;
      }
    }
    SBL_DelayMs(20U);
  }
}
