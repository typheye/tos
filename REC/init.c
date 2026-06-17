#include "rec.h"

#include "sbl_common.h"
#include "sbl_flash.h"
#include "sbl_hw.h"
#include "sbl_lcd.h"

#define SRE_CMD_ADDR       0x0801FC00UL
#define SRE_CMD_SIZE       1024UL
#define SRE_CMD_MAGIC      0x53524531UL /* SRE1 */
#define SRE_FORMAT_FAKE    1
#define SRE_IDLE_REBOOT_MS 60000UL
#define SRE_TITLE_Y        96U
#define SRE_STATUS_Y       120U
#define SRE_STATUS_H       16U

extern uint32_t HAL_GetTick(void);

typedef struct {
  uint32_t magic;
  uint32_t command;
  uint32_t arg;
  uint32_t crc;
} SRE_Command;

static const char sre_title[] SRE_CONST = "Recovery Mode";
static const char sre_wait[] SRE_CONST = "Awaiting instructions...";
static const char sre_format[] SRE_CONST = "Formatting userdata...";
static const char sre_done[] SRE_CONST = "Done. Rebooting...";
static const char sre_upgrade_fail[] SRE_CONST = "Upgrade failed: /init missing";

static SRE_CODE uint16_t sre_text_width(const char *text) {
  uint16_t len = 0U;
  while (text[len] && text[len] != '\n') {
    len++;
  }
  return (uint16_t)(len * 6U);
}

static SRE_CODE void sre_center(uint16_t y, const char *text, uint16_t color) {
  uint16_t w = sre_text_width(text);
  uint16_t x = (w >= SBL_LCD_W) ? 0U : (uint16_t)((SBL_LCD_W - w) / 2U);
  SBL_LcdDrawText(x, y, text, color, 1U);
}

static SRE_CODE void sre_draw_status(const char *status, uint16_t color) {
  SBL_LcdRect(0U, (uint16_t)(SRE_STATUS_Y - 2U), SBL_LCD_W,
              SRE_STATUS_H, SBL_BLACK);
  sre_center(SRE_STATUS_Y, status, color);
}

static SRE_CODE void sre_draw_full(const char *status, uint16_t color) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  sre_center(SRE_TITLE_Y, sre_title, SBL_WHITE);
  sre_center(SRE_STATUS_Y, status, color);
  SBL_LcdDisplayOn();
}

static SRE_CODE uint32_t sre_crc(const SRE_Command *cmd) {
  return cmd->magic ^ cmd->command ^ cmd->arg ^ 0xA55A5AA5UL;
}

static SRE_CODE uint8_t sre_read_command(SRE_Command *out) {
  const SRE_Command *cmd = (const SRE_Command *)SRE_CMD_ADDR;
  if (!out || cmd->magic != SRE_CMD_MAGIC || cmd->crc != sre_crc(cmd)) {
    return 0U;
  }
  *out = *cmd;
  return 1U;
}

static SRE_CODE void sre_erase_command(void) {
  if (SBL_FlashUnlock()) {
    (void)SBL_FlashProgramWord(SRE_CMD_ADDR, 0x00000000UL);
    SBL_FlashLock();
  }
}

static SRE_CODE void sre_format_userdata(void) {
  sre_draw_status(sre_format, SBL_RED);
  SBL_DelayMs(800U);
#if SRE_FORMAT_FAKE
  sre_draw_status(sre_done, SBL_GREEN);
  SBL_DelayMs(700U);
#endif
}

static SRE_CODE void sre_upgrade_from_sd(void) {
  sre_draw_status("Mounting SD...", SBL_WHITE);
  if (!SRE_FatProbeInit()) {
    sre_draw_status(sre_upgrade_fail, SBL_RED);
    SBL_DelayMs(1500U);
    return;
  }
  if (!SRE_FatHasUpgradeManifest()) {
    sre_draw_status("Upgrade failed: manifest missing", SBL_RED);
    SBL_DelayMs(1500U);
    return;
  }
  if (!SRE_FatFlashUpgrade(sre_draw_status)) {
    sre_draw_status("Upgrade failed", SBL_RED);
    SBL_DelayMs(1500U);
    return;
  }
  sre_draw_status("Upgrade done. Rebooting...", SBL_GREEN);
  SBL_DelayMs(1500U);
  SBL_SystemReboot();
}

SRE_CODE void SRE_Run(uint8_t mode) {
  SRE_Command cmd;
  uint32_t start_ms;

  SBL_LedsOff();
  SBL_LcdBacklightFull();
  sre_draw_full(sre_wait, SBL_WHITE);

  if (mode == REC_MODE_FORMAT) {
    sre_format_userdata();
    SBL_SystemReboot();
  } else if (mode == REC_MODE_UPGRADE) {
    sre_upgrade_from_sd();
  } else if (sre_read_command(&cmd)) {
    sre_erase_command();
    if (cmd.command == 1U) {
      sre_format_userdata();
      SBL_SystemReboot();
    } else if (cmd.command == 2U) {
      sre_upgrade_from_sd();
    }
  }

  start_ms = HAL_GetTick();
  while (1) {
    if (mode == REC_MODE_WAIT &&
        (uint32_t)(HAL_GetTick() - start_ms) >= SRE_IDLE_REBOOT_MS) {
      SBL_SystemReboot();
    }
    SBL_DelayMs(20U);
  }
}
