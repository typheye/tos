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
#include <stdlib.h>
#include <string.h>

#define CCMRAM __attribute__((section(".ccmram")))

extern USART boardSerial;
extern TSDIO boardSDIO;
extern LCD boardLCD;
extern KeyManager keyManager;
extern SD_HandleTypeDef hsd;

// ==================== GUI 菜单 ====================
#define SD_MENU_ITEMS 5
static const char *sd_menus[SD_MENU_ITEMS] = {
    "1. Mount FS", "2. Unmount FS", "3. List Files", "4. Refresh", "5. Back"};

static int sd_menu_select = 0;
// static SDCard_Status_t last_status = SD_CARD_ERROR;
// static uint32_t last_capacity = 0;
static CCMRAM char file_list[20][32];
static uint32_t file_count = 0;

// static void clear_area(int x, int y, int w, int h) {
//   PD_SetColor(LCD_COLOR_BLACK);
//   PD_SetFill(true);
//   PD_DrawRect(x, y, w, h);
//   PD_SetFill(false);
// }

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
    printf("  Found ");
    char buf[16];
    sprintf(buf, "%lu", count);
    printf(buf);
    printf(" files\r\n");
  } else {
    file_count = 0;
    printf("  Failed to list files\r\n");
  }
}

static void draw_status_bar(void) {
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);
  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "SD Card");
}

static void draw_sd_info(void) {
  char dbg[32];
  int y_offset = 30;
  SDCard_Status_t current_status = boardSDIO.getStatus();

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, y_offset, "SD Card:");

  const char *status_text;
  uint32_t status_color;
  switch (current_status) {
  case SD_CARD_OK:
    status_text = "Ready";
    status_color = LCD_COLOR_GREEN;
    break;
  case SD_CARD_NO_CARD:
    status_text = "No Card";
    status_color = LCD_COLOR_RED;
    break;
  default:
    status_text = "Error";
    status_color = LCD_COLOR_YELLOW;
    break;
  }
  PD_SetColor(status_color);
  PD_DrawString(80, y_offset, status_text);

  y_offset += 16;
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, y_offset, "Capacity:");
  if (current_status == SD_CARD_OK) {
    SDCard_Info_t info = boardSDIO.getInfo();
    sprintf(dbg, "%lu MB", info.capacity_mb);
    PD_SetColor(LCD_COLOR_CYAN);
    PD_DrawString(80, y_offset, dbg);
  } else {
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(80, y_offset, "N/A");
  }

  y_offset += 16;
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, y_offset, "FS State:");
  if (is_fs_mounted()) {
    PD_SetColor(LCD_COLOR_GREEN);
    PD_DrawString(80, y_offset, "Mounted");
  } else {
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(80, y_offset, "Not Mounted");
  }
}

static void draw_menu(void) {
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 78, 220, 110);
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 84, "Operations:");

  for (int i = 0; i < SD_MENU_ITEMS; i++) {
    int y = 98 + i * 16;
    if (i == sd_menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 14);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }
    PD_DrawString(20, y, sd_menus[i]);
  }
}

static void draw_bottom_bar(void) {
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 215, 240, 25);
  PD_SetFill(false);
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_CYAN);
  PD_DrawString(10, 219, "A8:Down");
  PD_DrawString(75, 219, "D0:Up");
  PD_DrawString(130, 219, "Enter:OK");
}

void sd_card_activity_gui(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  sd_menu_select = 0;
  file_count = 0;
  PD_FillScreen(LCD_COLOR_BLACK);
  printf("\r\n========== SD Card GUI Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sd_menu_select++;
      if (sd_menu_select >= SD_MENU_ITEMS)
        sd_menu_select = SD_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (sd_menu_select > 0)
        sd_menu_select--;
      HAL_Delay(150);
    }

    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      printf("Executing: ");
      printf(sd_menus[sd_menu_select]);
      printf("\r\n");

      switch (sd_menu_select) {
      case 0:
        if (FS_Mount("0:") == FS_OK) {
          printf("  [OK] Mounted\r\n");
          list_root_files();
        } else {
          printf("  [ERROR] Mount failed\r\n");
        }
        break;
      case 1:
        if (FS_Unmount("0:") == FS_OK) {
          printf("  [OK] Unmounted\r\n");
          file_count = 0;
        } else {
          printf("  [ERROR] Unmount failed\r\n");
        }
        break;
      case 2: // List Files
        if (is_fs_mounted()) {
          list_root_files();

          // ========== 单独的文件列表页面 ==========
          if (file_count > 0) {
            uint8_t exit_file_view = 0;
            uint32_t last_file_update = HAL_GetTick();

            // 进入文件列表显示页面
            while (!exit_file_view) {
              keyManager.btn_enter.tick();

              // 按 Enter 退出
              if (keyManager.btn_enter.getState() == KEY_PRESSED) {
                exit_file_view = 1;
                break;
              }

              // 每 200ms 刷新一次显示（列出所有文件）
              if (HAL_GetTick() - last_file_update > 200) {
                last_file_update = HAL_GetTick();

                // 清屏并显示文件列表
                PD_FillScreen(LCD_COLOR_BLACK);

                // 标题
                PD_SetColor(LCD_COLOR_BLUE);
                PD_SetFill(true);
                PD_DrawRect(0, 0, 240, 22);
                PD_SetFill(false);
                PD_SetFont(FONT_ASCII_16);
                PD_SetColor(LCD_COLOR_WHITE);
                PD_DrawString(10, 4, "Files (Enter:Exit)");

                // 显示文件列表
                PD_SetFont(FONT_ASCII_12);
                int y_pos = 35;
                for (uint32_t i = 0; i < file_count && i < 20; i++) {
                  if (y_pos > 210)
                    break;

                  // 交替颜色便于阅读
                  if (i % 2 == 0) {
                    PD_SetColor(LCD_COLOR_WHITE);
                  } else {
                    PD_SetColor(LCD_COLOR_CYAN);
                  }

                  // 截断过长文件名
                  char display_name[28];
                  if (strlen(file_list[i]) > 22) {
                    strncpy(display_name, file_list[i], 19);
                    display_name[19] = '\0';
                    strcat(display_name, "...");
                  } else {
                    strcpy(display_name, file_list[i]);
                  }
                  PD_DrawString(10, y_pos, display_name);
                  y_pos += 16;
                }

                // 底部提示
                PD_SetColor(LCD_COLOR_DARK_BLUE);
                PD_SetFill(true);
                PD_DrawRect(0, 215, 240, 25);
                PD_SetFill(false);
                PD_SetColor(LCD_COLOR_YELLOW);
                PD_DrawString(70, 219, "Press Enter to exit");

                LCD_Flush();
              }

              HAL_Delay(20);
            }
          }
          // ========== 文件列表页面结束 ==========

        } else {
          printf("  [ERROR] Filesystem not mounted\r\n");
        }
        break;
      case 3:
        printf("Refreshing...\r\n");
        // 不要重新初始化 SDIO，只刷新文件系统状态
        // boardSDIO.init();  // 删除这行，会导致卡住

        // 只需要重新检查挂载状态和刷新文件列表
        if (is_fs_mounted()) {
          list_root_files();
        } else {
          // 如果未挂载，尝试重新获取 SD 卡状态但不重新初始化
          SDCard_Status_t status = boardSDIO.getStatus();
          if (status == SD_CARD_OK) {
            printf("  SD card ready, but FS not mounted\r\n");
          } else {
            printf("  SD card not ready\r\n");
          }
        }
        break;
      case 4:
        printf("Exit SD Card GUI\r\n");
        return;
      }
    }
    last_enter_state = current_enter_state;

    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();
      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();
      draw_sd_info();
      draw_menu();
      draw_bottom_bar();
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}

void sd_card_activity(void) { sd_card_activity_gui(); }

// ==================== 硬件直接测试 ====================
void sd_card_direct_activity(void) {
  printf("\r\n========== SD Card Direct Test ==========\r\n");
  if (boardSDIO.directWriteTest()) {
    printf("  [OK] SD card functional\r\n");
  } else {
    printf("  [FAIL] SD card test failed\r\n");
  }
  printf("========== Activity Complete ==========\r\n");
}

// ==================== 完整诊断 ====================
void sd_card_diagnostic(void) {
  char dbg[128];
  printf("\r\n========== SD CARD DIAGNOSTIC ==========\r\n");

  // 1. 硬件状态
  HAL_SD_CardStateTypeDef state = HAL_SD_GetCardState(&hsd);
  sprintf(dbg, "Card state: %lu\r\n", (unsigned long)state);
  printf(dbg);

  // 2. 卡信息
  HAL_SD_CardInfoTypeDef info;
  if (HAL_SD_GetCardInfo(&hsd, &info) == HAL_OK) {
    uint64_t total_mb =
        (uint64_t)info.BlockNbr * info.BlockSize / (1024 * 1024);
    sprintf(dbg, "Card Type: %lu, Capacity: %llu MB\r\n",
            (unsigned long)info.CardType, total_mb);
    printf(dbg);
  }

  // 3. 挂载测试
  FATFS fs;
  FRESULT res = f_mount(&fs, "0:", 1);
  if (res == FR_OK) {
    printf("Mount: OK\r\n");
    DWORD free_clusters;
    FATFS *fs_info;
    if (f_getfree("0:", &free_clusters, &fs_info) == FR_OK) {
      uint64_t total = (uint64_t)(fs_info->n_fatent - 2) * fs_info->csize *
                       512 / (1024 * 1024);
      uint64_t free_mb =
          (uint64_t)free_clusters * fs_info->csize * 512 / (1024 * 1024);
      sprintf(dbg, "Total: %llu MB, Free: %llu MB\r\n", total, free_mb);
      printf(dbg);
    }
    f_mount(NULL, "0:", 0);
  } else {
    sprintf(dbg, "Mount failed: %d\r\n", res);
    printf(dbg);
  }

  printf("========== DIAGNOSTIC COMPLETE ==========\r\n");
}

// ==================== 读写测试 ====================
void sd_card_rw_test(void) {
  char dbg[128];
  printf("\r\n========== SD CARD RW TEST ==========\r\n");

  // 挂载
  FATFS fs;
  if (f_mount(&fs, "0:", 1) != FR_OK) {
    printf("Mount failed!\r\n");
    return;
  }

  // 写入测试文件
  FIL file;
  const char *test_data = "SD Card Test - Hello from STM32!\nLine 2\nLine 3\n";
  if (f_open(&file, "0:/rw_test.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
    UINT bw;
    f_write(&file, test_data, strlen(test_data), &bw);
    f_close(&file);
    printf("Write: OK\r\n");
  } else {
    printf("Write: FAILED\r\n");
  }

  // 读取测试文件
  if (f_open(&file, "0:/rw_test.txt", FA_READ) == FR_OK) {
    char buffer[256];
    UINT br;
    f_read(&file, buffer, sizeof(buffer) - 1, &br);
    buffer[br] = '\0';
    f_close(&file);
    printf("Read: OK\r\n");
    printf("Content:\r\n---\r\n");
    printf(buffer);
    printf("---\r\n");
  } else {
    printf("Read: FAILED\r\n");
  }

  // 列出目录
  printf("\nDirectory:\r\n");
  DIR dir;
  FILINFO fno;
  if (f_opendir(&dir, "0:") == FR_OK) {
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
      sprintf(dbg, "  %s (%lu bytes)\r\n", fno.fname, (unsigned long)fno.fsize);
      printf(dbg);
    }
    f_closedir(&dir);
  }

  f_mount(NULL, "0:", 0);
  printf("========== RW TEST COMPLETE ==========\r\n");
}

// ==================== 文件系统操作 ====================
void sd_card_mount(void) {
  printf("\r\nMounting...\r");
  if (FS_Mount("0:") == FS_OK) {
    printf("  [OK] Mounted\r\n");
    list_root_files();
  } else {
    printf("  [FAIL] Mount failed\r\n");
  }
}

void sd_card_unmount(void) {
  printf("\r\nUnmounting...\r");
  if (FS_Unmount("0:") == FS_OK) {
    printf("  [OK] Unmounted\r\n");
  } else {
    printf("  [FAIL] Unmount failed\r\n");
  }
}

void sd_card_list(void) {
  printf("\r\nListing...\r");
  if (is_fs_mounted()) {
    list_root_files();
  } else {
    printf("  Filesystem not mounted\r\n");
  }
}

void sd_card_format(void) {
  printf("\r\n========== SD CARD FORMAT ==========\r");
  printf("WARNING: This will erase ALL data!\r");
  printf("Press Enter to continue, D0 to cancel...\r");

  uint32_t start = HAL_GetTick();
  while (HAL_GetTick() - start < 5000) {
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      printf("Cancelled\r");
      return;
    }
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      break;
    HAL_Delay(50);
  }

  f_mount(NULL, "0:", 0);
  printf("Formatting...\r");

#define WORK_BUF_SIZE (32 * 1024)
  uint32_t *work = (uint32_t *)malloc(WORK_BUF_SIZE);
  if (!work) {
    printf("  No memory\r");
    return;
  }
  memset(work, 0, WORK_BUF_SIZE);

  FRESULT res = f_mkfs("0:", 0, 0, work, WORK_BUF_SIZE);
  char dbg[64];
  ;
  sprintf(dbg, "  f_mkfs result: %d\r", res);
  printf(dbg);
  free(work);

  if (res == FR_OK) {
    printf("  [OK] Format complete\r");
  } else {
    printf("  [FAIL] Format failed - please format on PC\r");
  }
  printf("========== FORMAT COMPLETE ==========\r");
}