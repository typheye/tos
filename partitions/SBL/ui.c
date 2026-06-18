#include "sbl_ui.h"

#include "build.h"
#include "sbl_hw.h"
#include "sbl_lcd.h"
#include "sbl_state.h"
#include "sbl_usb.h"

#define SBL_MENU_COUNT          3U
#define SBL_PRESS_LONG_MS     800U
#define SBL_PRESS_DEBOUNCE_MS  30U
#define SBL_IDLE_REBOOT_MS 300000UL
#define SBL_UNLOCK_TIMEOUT_MS 30000UL

#define SBL_TITLE_Y            40U
#define SBL_LEFT_X             20U
#define SBL_HELP_Y             66U
#define SBL_MENU_Y            126U
#define SBL_INFO_Y            150U
#define SBL_MENU_BG_Y         120U
#define SBL_MENU_BG_H          18U
#define SBL_UNLOCK_TITLE_Y     36U
#define SBL_UNLOCK_BODY_Y      60U
#define SBL_UNLOCK_HELP_Y     146U
#define SBL_UNLOCK_CHOICE_Y   190U
#define SBL_UNLOCK_YES_X       64U
#define SBL_UNLOCK_NO_X       154U

#define SBL_FONT_SMALL          1U

extern uint32_t HAL_GetTick(void);

typedef struct {
  uint8_t unlocked;
} SBL_PageParams;

static SBL_PageParams sbl_page_storage;
static SBL_PageParams *sbl_page_params = &sbl_page_storage;
static uint8_t sbl_fastboot_visible;

static const char sbl_txt_title[] SBL_CONST = "FASTBOOT MENU";
static const char sbl_txt_help[] SBL_CONST =
    "select:\nshort press the button\ncontinue:\nlong press the button";
static const char sbl_txt_reboot[] SBL_CONST = "Reboot";
static const char sbl_txt_recovery[] SBL_CONST = "Reboot to recovery";
static const char sbl_txt_bootloader[] SBL_CONST = "Reboot to bootloader";
static const char sbl_txt_product[] SBL_CONST =
    "product_name:" SBL_BUILD_PRODUCT_NAME;
static const char sbl_txt_version[] SBL_CONST =
    "version:" SBL_BUILD_VERSION;
static const char sbl_txt_bootver[] SBL_CONST =
    "version-bootloader:" SBL_BUILD_VERSION_BOOTLOADER;
static const char sbl_txt_baseband[] SBL_CONST =
    "version-baseband:" SBL_BUILD_VERSION_BASEBAND;
static const char sbl_txt_unlocked[] SBL_CONST = "unlocked:";
static const char sbl_txt_yes[] SBL_CONST = "yes";
static const char sbl_txt_no[] SBL_CONST = "no";
static const char sbl_unlock_title[] SBL_CONST = "Unlock bootloader";
static const char sbl_unlock_body[] SBL_CONST =
    "This operation will delete all\n"
    "personal data on your device to prevant\n"
    "unauthorized access,then you can\n"
    "install new openrating system software\n"
    "on the device.";
static const char sbl_unlock_help[] SBL_CONST =
    "select: short press the button\n"
    "continue:long press the button";
static const char sbl_unlock_yes[] SBL_CONST = "YES";
static const char sbl_unlock_no[] SBL_CONST = "NO";
static const char sbl_damage_title[] SBL_CONST = "SYSTEM DAMAGE";
static const char sbl_damage_body[] SBL_CONST =
    "The system has some errors.";
static const char sbl_rec_exception_title[] SBL_CONST = "RECOVERY EXCEPTION";
static const char sbl_rec_exception_body[] SBL_CONST =
    "The recovery has some errors.";

static const char *const sbl_menus[SBL_MENU_COUNT] SBL_CONST = {
    sbl_txt_reboot,
    sbl_txt_recovery,
    sbl_txt_bootloader,
};

static SBL_CODE uint16_t sbl_title_width(const char *text) {
  uint16_t len = 0U;
  while (text[len] && text[len] != '\n') {
    len++;
  }
  return (uint16_t)(len * 9U);
}

static SBL_CODE uint16_t sbl_text_width(const char *text, uint8_t scale) {
  uint16_t len = 0U;
  while (text[len] && text[len] != '\n') {
    len++;
  }
  return (uint16_t)(len * 6U * scale);
}

static SBL_CODE void sbl_draw_text_center_line(uint16_t y, const char *text,
                                               uint16_t color,
                                               uint8_t scale) {
  uint16_t w = sbl_text_width(text, scale);
  uint16_t x = (w >= SBL_LCD_W) ? 0U : (uint16_t)((SBL_LCD_W - w) / 2U);
  SBL_LcdDrawText(x, y, text, color, scale);
}

static SBL_CODE void sbl_draw_text_center_block(uint16_t y, const char *text,
                                                uint16_t color,
                                                uint8_t scale) {
  const char *p = text;
  char line[44];
  while (*p) {
    uint16_t n = 0U;
    while (p[n] && p[n] != '\n' && n < (uint16_t)(sizeof(line) - 1U)) {
      line[n] = p[n];
      n++;
    }
    line[n] = 0;
    sbl_draw_text_center_line(y, line, color, scale);
    y = (uint16_t)(y + 12U * scale);
    p = (p[n] == '\n') ? &p[n + 1U] : &p[n];
  }
}

static SBL_CODE void sbl_draw_center_title(void) {
  uint16_t w = sbl_title_width(sbl_txt_title);
  uint16_t x = (w >= SBL_LCD_W) ? 0U : (uint16_t)((SBL_LCD_W - w) / 2U);
  SBL_LcdDrawTitleText(x, SBL_TITLE_Y, sbl_txt_title, SBL_WHITE);
}

static SBL_CODE void sbl_draw_menu(uint8_t selected) {
  SBL_LcdRect(0U, SBL_MENU_BG_Y, SBL_LCD_W, SBL_MENU_BG_H, SBL_BLACK);
  SBL_LcdDrawText(SBL_LEFT_X, SBL_MENU_Y, sbl_menus[selected], SBL_RED,
                  SBL_FONT_SMALL);
}

static SBL_CODE void sbl_draw_info(void) {
  uint8_t unlocked = sbl_page_params ? sbl_page_params->unlocked
                                     : SBL_StateUnlocked();
  uint16_t y = SBL_INFO_Y;
  SBL_LcdDrawText(SBL_LEFT_X, y, sbl_txt_product, SBL_WHITE, SBL_FONT_SMALL);
  y = (uint16_t)(y + 12U);
  SBL_LcdDrawText(SBL_LEFT_X, y, sbl_txt_version, SBL_WHITE, SBL_FONT_SMALL);
  y = (uint16_t)(y + 12U);
  SBL_LcdDrawText(SBL_LEFT_X, y, sbl_txt_bootver, SBL_WHITE, SBL_FONT_SMALL);
  y = (uint16_t)(y + 12U);
  SBL_LcdDrawText(SBL_LEFT_X, y, sbl_txt_baseband, SBL_WHITE, SBL_FONT_SMALL);
  y = (uint16_t)(y + 12U);
  SBL_LcdDrawText(SBL_LEFT_X, y, sbl_txt_unlocked, SBL_WHITE, SBL_FONT_SMALL);
  SBL_LcdDrawText((uint16_t)(SBL_LEFT_X + 54U), y,
                  unlocked ? sbl_txt_yes : sbl_txt_no,
                  SBL_WHITE, SBL_FONT_SMALL);
}

static SBL_CODE void sbl_load_page_params(void) {
  sbl_page_params->unlocked = SBL_StateUnlocked();
}

static SBL_CODE void sbl_redraw_fastboot_hidden(uint8_t *selected) {
  SBL_LcdDisplayOff();
  sbl_load_page_params();
  if (selected) {
    *selected = 0U;
  }
  SBL_UiDrawFastboot();
}

static SBL_CODE void sbl_return_fastboot_quick(uint8_t *selected) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  SBL_DelayMs(320U);
  sbl_redraw_fastboot_hidden(selected);
  SBL_WaitButtonRelease(120U);
}

static SBL_CODE void sbl_draw_unlock_choices(uint8_t yes_selected) {
  SBL_LcdRect(40U, (uint16_t)(SBL_UNLOCK_CHOICE_Y - 2U), 160U, 22U,
              SBL_BLACK);
  SBL_LcdDrawTitleText(SBL_UNLOCK_YES_X, SBL_UNLOCK_CHOICE_Y,
                       sbl_unlock_yes,
                       yes_selected ? SBL_RED : SBL_WHITE);
  SBL_LcdDrawTitleText(SBL_UNLOCK_NO_X, SBL_UNLOCK_CHOICE_Y,
                       sbl_unlock_no,
                       yes_selected ? SBL_WHITE : SBL_RED);
}

static SBL_CODE void sbl_draw_unlock_page(void) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 26U, SBL_LCD_W, 194U, SBL_BLACK);
  sbl_draw_text_center_line(SBL_UNLOCK_TITLE_Y, sbl_unlock_title, SBL_WHITE,
                            SBL_FONT_SMALL);
  sbl_draw_text_center_block(SBL_UNLOCK_BODY_Y, sbl_unlock_body, SBL_WHITE,
                             SBL_FONT_SMALL);
  SBL_LcdDrawText(20U, SBL_UNLOCK_HELP_Y, sbl_unlock_help, SBL_WHITE,
                  SBL_FONT_SMALL);
  sbl_draw_unlock_choices(1U);
  SBL_LcdDisplayOn();
}

static SBL_CODE void sbl_run_unlock_page(uint8_t *selected) {
  uint8_t yes_selected = 1U;
  uint8_t was_down = 0U;
  uint8_t long_done = 0U;
  uint32_t press_start = 0U;
  uint32_t last_action = HAL_GetTick();

  sbl_draw_unlock_page();
  SBL_WaitButtonRelease(120U);
  last_action = HAL_GetTick();

  while (1) {
    SBL_USB_Tick();
    uint8_t down = SBL_IsButtonDown();
    uint32_t now = HAL_GetTick();

    if ((uint32_t)(now - last_action) >= SBL_UNLOCK_TIMEOUT_MS) {
      SBL_USB_WriteTextWait("FAIL unlock timeout\r\n");
      sbl_return_fastboot_quick(selected);
      return;
    }

    if (down && !was_down) {
      last_action = now;
      press_start = now;
      long_done = 0U;
      SBL_DelayMs(SBL_PRESS_DEBOUNCE_MS);
    } else if (down && !long_done &&
               (uint32_t)(now - press_start) >= SBL_PRESS_LONG_MS) {
      uint8_t ok = 0U;
      long_done = 1U;
      SBL_LcdDisplayOff();
      SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
      SBL_DelayMs(180U);
      if (yes_selected) {
        ok = SBL_StateSetUnlocked(1U);
        if (ok && sbl_page_params) {
          sbl_page_params->unlocked = 1U;
        }
      }
      SBL_USB_SendUnlockResult(yes_selected, ok);
      SBL_DelayMs(220U);
      if (yes_selected && ok) {
        SBL_SystemReboot();
      }
      sbl_return_fastboot_quick(selected);
      return;
    } else if (!down && was_down) {
      last_action = now;
      if (!long_done && (uint32_t)(now - press_start) >= SBL_PRESS_DEBOUNCE_MS) {
        yes_selected = yes_selected ? 0U : 1U;
        sbl_draw_unlock_choices(yes_selected);
      }
      SBL_DelayMs(SBL_PRESS_DEBOUNCE_MS);
    }

    was_down = down;
    SBL_DelayMs(10U);
  }
}

SBL_CODE void SBL_UiDrawFastboot(void) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  sbl_load_page_params();
  sbl_draw_center_title();
  SBL_LcdDrawText(SBL_LEFT_X, SBL_HELP_Y, sbl_txt_help, SBL_WHITE,
                  SBL_FONT_SMALL);
  sbl_draw_menu(0U);
  sbl_draw_info();
  SBL_LcdDisplayOn();
  sbl_fastboot_visible = 1U;
}

SBL_CODE void SBL_UiRunFastboot(void) {
  uint8_t selected = 0U;
  uint8_t was_down = 0U;
  uint8_t long_done = 0U;
  uint32_t press_start = 0U;
  uint32_t last_action = HAL_GetTick();
  uint32_t usb_grace_start = last_action;
  uint32_t usb_retry_at = last_action + 2500U;
  uint8_t usb_grace_done = 0U;

  if (sbl_fastboot_visible) {
    sbl_load_page_params();
  } else {
    sbl_redraw_fastboot_hidden(&selected);
  }
  SBL_WaitButtonRelease(120U);
  usb_grace_start = HAL_GetTick();
  usb_retry_at = usb_grace_start + 2500U;

  while (1) {
    uint32_t now = HAL_GetTick();
    if (!usb_grace_done && !SBL_USB_IsConfigured() &&
        (uint32_t)(now - usb_grace_start) >= 2500U) {
      SBL_USB_DeInit();
      usb_grace_done = 1U;
      usb_retry_at = now + 1200U;
    }
    if (usb_grace_done && !SBL_USB_IsConfigured() &&
        (int32_t)(now - usb_retry_at) >= 0) {
      SBL_USB_DisconnectPulse();
      if (SBL_USB_Init()) {
        usb_grace_done = 0U;
        usb_grace_start = HAL_GetTick();
        usb_retry_at = usb_grace_start + 2500U;
      } else {
        SBL_USB_DeInit();
        usb_retry_at = HAL_GetTick() + 1200U;
      }
    }
    SBL_USB_Tick();
    if (SBL_USB_IsBusy()) {
      was_down = 0U;
      long_done = 0U;
      SBL_DelayMs(4U);
      continue;
    }
    if (SBL_USB_ConsumeUnlockRequest()) {
      sbl_run_unlock_page(&selected);
      last_action = HAL_GetTick();
      was_down = 0U;
      long_done = 0U;
      continue;
    }
    if (SBL_USB_ConsumeBootloaderReloadRequest()) {
      sbl_redraw_fastboot_hidden(&selected);
      last_action = HAL_GetTick();
      was_down = 0U;
      long_done = 0U;
      continue;
    }
    uint8_t down = SBL_IsButtonDown();
    now = HAL_GetTick();

    if ((uint32_t)(now - last_action) >= SBL_IDLE_REBOOT_MS) {
      SBL_SystemReboot();
    }

    if (down && !was_down) {
      last_action = now;
      press_start = now;
      long_done = 0U;
      SBL_DelayMs(SBL_PRESS_DEBOUNCE_MS);
    } else if (down && !long_done &&
               (uint32_t)(now - press_start) >= SBL_PRESS_LONG_MS) {
      long_done = 1U;
      last_action = now;
      if (selected == 0U) {
        SBL_SystemReboot();
      } else if (selected == 1U) {
        if (SBL_StateSetBootTarget(SBL_BOOT_TARGET_RECOVERY)) {
          SBL_SystemReboot();
        }
      } else if (selected == 2U) {
        if (SBL_StateSetBootTarget(SBL_BOOT_TARGET_FASTBOOT)) {
          SBL_SystemReboot();
        }
      }
    } else if (!down && was_down) {
      last_action = now;
      if (!long_done && (uint32_t)(now - press_start) >= SBL_PRESS_DEBOUNCE_MS) {
        selected = (uint8_t)((selected + 1U) % SBL_MENU_COUNT);
        sbl_draw_menu(selected);
      }
      SBL_DelayMs(SBL_PRESS_DEBOUNCE_MS);
    }

    was_down = down;
    SBL_DelayMs(10U);
  }
}

SBL_CODE void SBL_UiRunSystemDamage(void) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  sbl_draw_text_center_line(102U, sbl_damage_title, SBL_RED, SBL_FONT_SMALL);
  sbl_draw_text_center_block(126U, sbl_damage_body, SBL_WHITE, SBL_FONT_SMALL);
  SBL_LcdDisplayOn();
  while (1) {
    SBL_USB_Tick();
    SBL_DelayMs(10U);
  }
}

SBL_CODE void SBL_UiRunRecoveryException(void) {
  SBL_LcdDisplayOff();
  SBL_LcdRect(0U, 0U, SBL_LCD_W, SBL_LCD_H, SBL_BLACK);
  sbl_draw_text_center_line(102U, sbl_rec_exception_title, SBL_RED,
                            SBL_FONT_SMALL);
  sbl_draw_text_center_block(126U, sbl_rec_exception_body, SBL_WHITE,
                             SBL_FONT_SMALL);
  SBL_LcdDisplayOn();
  while (1) {
    SBL_USB_Tick();
    SBL_DelayMs(10U);
  }
}
