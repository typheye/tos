#include "demo/include/3dox_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/lib3dox.h"
#include "include/libpd.h"
#include <stdio.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;

// 渲染区域（全屏）
#ifndef RENDER_WIDTH
#define RENDER_WIDTH LCD_WIDTH
#endif

#ifndef RENDER_HEIGHT
#define RENDER_HEIGHT LCD_HEIGHT
#endif

// 帧缓冲区
static uint16_t *framebuffer = NULL;
static int render_frame = 0;

// 像素回调函数 - 写入帧缓冲区
static void on_pixel(int x, int y, uint32_t color) {
  if (framebuffer && x >= 0 && x < RENDER_WIDTH && y >= 0 &&
      y < RENDER_HEIGHT) {
    // 将 RGB888 转换为 RGB565
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    framebuffer[y * LCD_WIDTH + x] = rgb565;
  }
}

// 绘制界面（信息显示在左上角）
static void draw_ui(int progress, int frame) {
  char dbg[32];

  // 保存当前绘图状态（临时叠加文字到渲染图像上）
  PD_Init();

  // 左上角显示信息 - 背景框
  int text_x = 20;
  int text_y = 210;
  int box_w = 200;
  int box_h = 22;

  // 绘制半透明背景框
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(text_x, text_y, box_w, box_h);
  PD_SetFill(false);

  // 绘制边框
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(text_x, text_y, box_w, box_h);

  // 显示 Frame（白色）
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);
  sprintf(dbg, "Frame: %d", frame);
  PD_DrawString(text_x + 5, text_y + 5, dbg);

  // 显示进度（绿色）
  if (progress < 100) {
    PD_SetColor(LCD_COLOR_GREEN);
    sprintf(dbg, "Process: %d%%", progress);
    PD_DrawString(text_x + 100, text_y + 5, dbg);
  } else {
    PD_SetColor(LCD_COLOR_GREEN);
    PD_DrawString(text_x + 100, text_y + 5, "Process: OK");
  }

  LCD_Flush();
}

// 光锥渲染主函数（持续渲染直到用户退出，不清除旧帧）
void render_3dox_activity(void) {
  int last_progress = -1;
  uint32_t last_update = HAL_GetTick();
  uint8_t last_enter_state = 0;
  int total_pixels = RENDER_WIDTH * RENDER_HEIGHT;
  int current_frame = 0;

  printf("\r\n========== 3D Path Tracer Started ==========\r\n");
  printf("Rendering Cornell Box with path tracing...\r\n");
  printf("Press Enter to stop rendering\r\n");

  // 初始化 PD 图形库
  PD_Init();

  // 获取帧缓冲区
  framebuffer = LCD_GetFrameBuffer();
  if (!framebuffer) {
    printf("[ERROR] Failed to get framebuffer!\r\n");
    return;
  }

  // 清屏
  PD_FillScreen(LCD_COLOR_BLACK);
  LCD_Flush();

  // 初始化渲染器
  render_init();
  render_frame = 0;

  printf("Rendering in progress...\r\n");

  // 持续渲染多帧（不清除旧帧，累积采样）
  while (1) {
    current_frame++;
    int pixels_rendered = 0;
    last_progress = -1;

    // 渲染当前帧的所有像素（不清除帧缓冲区，直接在原图上叠加采样）
    while (pixels_rendered < total_pixels) {
      // 检查退出条件（Enter 键）
      keyManager.btn_enter.tick();
      uint8_t current_enter =
          (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
      if (current_enter == 1 && last_enter_state == 0) {
        printf("\r\nRender stopped by user at frame ");
        char buf[16];
        sprintf(buf, "%d\r\n", current_frame);
        printf(buf);
        goto exit_render;
      }
      last_enter_state = current_enter;

      // 渲染一个像素（累积采样，不清除旧值）
      render_step(on_pixel);
      pixels_rendered++;

      // 更新进度显示
      int progress = (pixels_rendered * 100) / total_pixels;
      if (progress != last_progress || HAL_GetTick() - last_update > 50) {
        last_progress = progress;
        last_update = HAL_GetTick();
        draw_ui(progress, current_frame);
      }

      // 小延迟避免CPU过载
      if (pixels_rendered % 200 == 0) {
        HAL_Delay(1);
      }
    }

    // 当前帧渲染完成
    render_frame = current_frame;
    printf("Frame ");
    char buf[16];
    sprintf(buf, "%d", render_frame);
    printf(buf);
    printf(" complete\r\n");

    // 显示完成状态（帧结束时显示一次）
    draw_ui(100, current_frame);
    LCD_Flush();
  }

exit_render:
  // 显示停止信息
  PD_Init();
  PD_SetColor(LCD_COLOR_DARK_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetFont(FONT_ASCII_12);
  PD_DrawString(10, 6, "Render Stopped");
  PD_SetColor(LCD_COLOR_CYAN);
  char dbg[32];
  sprintf(dbg, "Frames: %d", render_frame);
  PD_DrawString(120, 6, dbg);
  LCD_Flush();

  // 等待2秒后退出
  HAL_Delay(2000);

  printf("Total frames rendered: ");
  char buf[16];
  sprintf(buf, "%d\r\n", render_frame);
  printf(buf);
  printf("3D Path Tracer Activity Exit\r\n");
}

// 带退出提示的版本
void render_3dox_activity_with_exit(void) { render_3dox_activity(); }