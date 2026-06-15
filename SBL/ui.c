#include "sbl_ui.h"

#include "build.h"
#include "sbl_hw.h"
#include "sbl_lcd.h"

#define SBL_MENU_COUNT          3U
#define SBL_PRESS_LONG_MS     800U
#define SBL_PRESS_DEBOUNCE_MS  30U

#define SBL_TITLE_Y            40U
#define SBL_LEFT_X             30U
#define SBL_HELP_Y             66U
#define SBL_MENU_Y            126U
#define SBL_INFO_Y            150U
#define SBL_MENU_BG_Y         120U
#define SBL_MENU_BG_H          18U

#define SBL_FONT_SMALL          1U

extern uint32_t HAL_GetTick(void);

static const char sbl_txt_title[] SBL_CONST = "FASTBOOT MENU";
static const char sbl_txt_help[] SBL_CONST =
    "select:\nshort press the button\ncontinue:\nlong press the button";
static const char sbl_txt_reboot[] SBL_CONST = "Reboot";
static const char sbl_txt_recovery[] SBL_CONST = "Reboot to recovery";
static const char sbl_txt_bootloader[] SBL_CONST = "Reboot to bootloader";
static const char sbl_txt_info[] SBL_CONST =
    "product_name:" SBL_BUILD_PRODUCT_NAME "\n"
    "version:" SBL_BUILD_VERSION "\n"
    "version-bootloader:" SBL_BUILD_VERSION_BOOTLOADER "\n"
    "version-baseband:" SBL_BUILD_VERSION_BASEBAND "\n"
    "unlocked:" SBL_BUILD_UNLOCKED;

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

SBL_CODE void SBL_UiDrawFastboot(void) {
  SBL_LcdRect(0U, 28U, SBL_LCD_W, 178U, SBL_BLACK);
  sbl_draw_center_title();
  SBL_LcdDrawText(SBL_LEFT_X, SBL_HELP_Y, sbl_txt_help, SBL_WHITE,
                  SBL_FONT_SMALL);
  sbl_draw_menu(0U);
  SBL_LcdDrawText(SBL_LEFT_X, SBL_INFO_Y, sbl_txt_info, SBL_WHITE,
                  SBL_FONT_SMALL);
}

SBL_CODE void SBL_UiRunFastboot(void) {
  uint8_t selected = 0U;
  uint8_t was_down = 0U;
  uint8_t long_done = 0U;
  uint32_t press_start = 0U;

  SBL_UiDrawFastboot();
  SBL_WaitButtonRelease(120U);

  while (1) {
    uint8_t down = SBL_IsButtonDown();
    uint32_t now = HAL_GetTick();

    if (down && !was_down) {
      press_start = now;
      long_done = 0U;
      SBL_DelayMs(SBL_PRESS_DEBOUNCE_MS);
    } else if (down && !long_done &&
               (uint32_t)(now - press_start) >= SBL_PRESS_LONG_MS) {
      long_done = 1U;
      if (selected == 0U) {
        SBL_SystemReboot();
      } else if (selected == 1U) {
        SBL_WaitButtonRelease(250U);
      } else if (selected == 2U) {
        SBL_DelayMs(1000U);
        selected = 0U;
        SBL_UiDrawFastboot();
        SBL_WaitButtonRelease(120U);
      }
    } else if (!down && was_down) {
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
