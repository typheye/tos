#include "include/sd_activity.hpp"
#include "bsp_driver_sd.h"
#include "fatfs.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/tsdio.hpp"
#include "hardware/include/usart.hpp"
#include "include/libfs.h"
#include "include/libpd.h"
#include <stdint.h>
#include <stdio.h>
#include "syslog.h"
#include <stdlib.h>
#include <string.h>

#define CCMRAM __attribute__((section(".ccmram")))

extern USART boardSerial;
extern TSDIO boardSDIO;
extern LCD boardLCD;
extern KeyManager keyManager;
extern SD_HandleTypeDef hsd;

#define SD_MENU_ITEMS 5
static const char *sd_menus[SD_MENU_ITEMS] = {
    "01 Mount", "02 Unmount", "03 List", "04 Refresh", "05 Back"};

static int sd_menu_select = 0;
static CCMRAM char file_list[20][32];
static uint32_t file_count = 0;

static int is_fs_mounted(void) {
  FS_DirHandle dir;
  if (FS_OpenDir(&dir, "0:") == FS_OK) {
    FS_CloseDir(dir);
    return 1;
  }
  return 0;
}

static void list_root_files(void) {
  FS_FileInfo_t files[20];
  uint32_t count = 0;
  if (FS_ListDir("0:", files, 20, &count) == FS_OK) {
    file_count = count;
    for (uint32_t i = 0; i < count && i < 20; i++) {
      strncpy(file_list[i], files[i].name, 31);
      file_list[i][31] = '\0';
    }
  } else {
    file_count = 0;
  }
}

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}

static void draw_sd_info(void) {
  char dbg[32];
  SDCard_Status_t current_status = boardSDIO.getStatus();

  PD_DrawAngledCard(8, 44, 224, 56, 6, TOS_CARD_BG);
  PD_SetFont(FONT_ASCII_16);

  PD_SetColor(LV_TEXT_HINT);
  PD_DrawString(16, 50, "Status");

  const char *status_text;
  uint32_t status_color;
  switch (current_status) {
  case SD_CARD_OK:     status_text = "Ready";   status_color = LV_SUCCESS; break;
  case SD_CARD_NO_CARD: status_text = "No Card"; status_color = LV_ERROR; break;
  default:             status_text = "Error";   status_color = LV_WARNING; break;
  }
  PD_SetColor(status_color);
  PD_DrawString(120, 50, status_text);

  PD_SetColor(LV_TEXT_HINT);
  PD_DrawString(16, 72, "Capacity");
  if (current_status == SD_CARD_OK) {
    SDCard_Info_t info = boardSDIO.getInfo();
    sprintf(dbg, "%lu MB", info.capacity_mb);
    PD_SetColor(LV_ACCENT);
    PD_DrawString(120, 72, dbg);
  } else {
    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(120, 72, "N/A");
  }

  PD_SetColor(LV_TEXT_HINT);
  PD_DrawString(16, 94, "FS");
  if (is_fs_mounted()) {
    PD_SetColor(LV_SUCCESS);
    PD_DrawString(120, 94, "Mounted");
  } else {
    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(120, 94, "Not Mounted");
  }
}

static void menu_cards(int sel) {
  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < SD_MENU_ITEMS; i++) {
    int cy = 33 + i * 25;
    if (i == sel) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, sd_menus[i]);
  }
}

void sd_card_activity_gui(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();

  PD_Init();
  sd_menu_select = 0;
  file_count = 0;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sd_menu_select++;
      if (sd_menu_select >= SD_MENU_ITEMS) sd_menu_select = SD_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (sd_menu_select > 0) sd_menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (sd_menu_select) {
      case 0: FS_Mount("0:"); list_root_files(); break;
      case 1: FS_Unmount("0:"); file_count = 0; break;
      case 2:
        if (is_fs_mounted()) {
          list_root_files();
          if (file_count > 0) {
            uint8_t exit_file_view = 0;
            uint32_t lfu = HAL_GetTick();
            while (!exit_file_view) {
              keyManager.btn_enter.tick();
              if (keyManager.btn_enter.getState() == KEY_PRESSED) {
                exit_file_view = 1; break;
              }
              if (HAL_GetTick() - lfu > 200) {
                lfu = HAL_GetTick();
                LCD_FLUSH({
                  PD_FillScreen(LV_BG_DARK);
                  bar("02");

                  PD_DrawAngledCard(8, 44, 224, 160, 6, TOS_CARD_BG);
                  PD_SetFont(FONT_ASCII_16);
                  int y_pos = 52;
                  for (uint32_t i = 0; i < file_count && i < 14; i++) {
                    if (y_pos > 195) break;
                    PD_SetColor((i % 2 == 0) ? LV_TEXT_PRIMARY : LV_ACCENT);
                    char display_name[28];
                    if (strlen(file_list[i]) > 22) {
                      strncpy(display_name, file_list[i], 19);
                      display_name[19] = '\0';
                      strcat(display_name, "...");
                    } else {
                      strcpy(display_name, file_list[i]);
                    }
                    PD_DrawString(16, y_pos, display_name);
                    y_pos += 15;
                  }
                  bbar("EXIT", NULL, NULL);
                });
              }
              HAL_Delay(1);
            }
          }
        }
        break;
      case 3:
        if (is_fs_mounted()) { list_root_files(); }
        break;
      case 4: return;
      }
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      LCD_FLUSH({
        PD_FillScreen(LV_BG_DARK);
        bar("02");
        draw_sd_info();
        menu_cards(sd_menu_select);
        bbar("ENTER", NULL, "UP/DOWN");
      });
    }
    HAL_Delay(1);
  }
}

void sd_card_activity(void) { sd_card_activity_gui(); }

void sd_card_direct_activity(void) {
  LOG_I("SDAC", "SD Card Direct Test");
  if (boardSDIO.directWriteTest())
    LOG_I("SDAC", "SD card functional");
  else
    LOG_E("SDAC", "SD card test failed");
  LOG_I("SDAC", "Activity Complete");
}

void sd_card_diagnostic(void) {
  LOG_I("SDAC", "SD Card Diagnostic");
  LOG_I("SDAC", "Card state: %lu", (unsigned long)HAL_SD_GetCardState(&hsd));

  HAL_SD_CardInfoTypeDef info;
  if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
    LOG_I("SDAC", "Card Type: %lu, Capacity: %llu MB", (unsigned long)info.CardType,
           (uint64_t)info.BlockNbr * info.BlockSize / (1024 * 1024));
  }

  FATFS fs;
  FRESULT res = f_mount(&fs, "0:", 1);
  if (res == FR_OK) {
    LOG_I("SDAC", "Mount: OK");
    DWORD free_clusters;
    FATFS *fs_info;
    if (f_getfree("0:", &free_clusters, &fs_info) == FR_OK) {
      LOG_I("SDAC", "Total: %llu MB, Free: %llu MB",
             (uint64_t)(fs_info->n_fatent - 2) * fs_info->csize * 512 / (1024 * 1024),
             (uint64_t)free_clusters * fs_info->csize * 512 / (1024 * 1024));
    }
    f_mount(NULL, "0:", 0);
  }
  LOG_I("SDAC", "Diagnostic complete");
}

void sd_card_rw_test(void) {
  LOG_I("SDAC", "SD Card RW Test");
  FATFS fs;
  if (f_mount(&fs, "0:", 1) != FR_OK) { LOG_E("SDAC", "Mount failed"); return; }

  FIL file;
  const char *test_data = "SD Card Test - Hello from STM32!\nLine 2\nLine 3\n";
  if (f_open(&file, "0:/rw_test.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
    UINT bw; f_write(&file, test_data, strlen(test_data), &bw); f_close(&file);
    LOG_I("SDAC", "Write: OK");
  }

  if (f_open(&file, "0:/rw_test.txt", FA_READ) == FR_OK) {
    char buffer[256]; UINT br;
    f_read(&file, buffer, sizeof(buffer) - 1, &br);
    buffer[br] = '\0'; f_close(&file);
    LOG_I("SDAC", "Read: OK\nContent:\n---\n%s---", buffer);
  }

  DIR dir; FILINFO fno;
  if (f_opendir(&dir, "0:") == FR_OK) {
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
      LOG_I("SDAC", "%s (%lu bytes)", fno.fname, (unsigned long)fno.fsize);
    }
    f_closedir(&dir);
  }
  f_mount(NULL, "0:", 0);
  LOG_I("SDAC", "RW Test complete");
}

void sd_card_mount(void) {
  if (FS_Mount("0:") == FS_OK) { LOG_I("SDAC", "Mounted"); list_root_files(); }
  else LOG_E("SDAC", "Mount failed");
}
void sd_card_unmount(void) {
  if (FS_Unmount("0:") == FS_OK) LOG_I("SDAC", "Unmounted");
  else LOG_E("SDAC", "Unmount failed");
}
void sd_card_list(void) {
  if (is_fs_mounted()) list_root_files();
  else LOG_W("SDAC", "Filesystem not mounted");
}

void sd_card_format(void) {
  LOG_I("SDAC", "SD Card Format");
  LOG_W("SDAC", "This will erase ALL data");
  LOG_I("SDAC", "Press Enter to continue, D0 to cancel...");
  uint32_t start = HAL_GetTick();
  while (HAL_GetTick() - start < 5000) {
    keyManager.collision_D0.tick(); keyManager.btn_enter.tick();
    if (keyManager.collision_D0.getState() == KEY_PRESSED) { LOG_I("SDAC", "Cancelled"); return; }
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;
    HAL_Delay(50);
  }
  f_mount(NULL, "0:", 0);
  LOG_I("SDAC", "Formatting...");

#define WORK_BUF_SIZE (32 * 1024)
  uint32_t *work = (uint32_t *)malloc(WORK_BUF_SIZE);
  if (!work) { LOG_E("SDAC", "No memory"); return; }
  memset(work, 0, WORK_BUF_SIZE);
  FRESULT res = f_mkfs("0:", 0, 0, work, WORK_BUF_SIZE);
  LOG_I("SDAC", "f_mkfs result: %d", res);
  free(work);
  if (res == FR_OK) LOG_I("SDAC", "Format complete");
  else LOG_E("SDAC", "Format failed");
  LOG_I("SDAC", "Format operation complete");
}
