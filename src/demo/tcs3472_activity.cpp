#include "include/tcs3472_activity.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/usart.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/tcs3472.hpp"
#include <stdio.h>
#include <string.h>

extern USART boardSerial;
extern KeyManager keyManager;
extern LCD boardLCD;
extern TCS3472 boardTCS3472;

// ==================== 菜单定义 ====================
#define TCS3472_MENU_ITEMS 5
static const char *menus[TCS3472_MENU_ITEMS] = {
    "1. Read Color", "2. Color Demo (LED)", "3. CCT & Lux", "4. Chart Mode",
    "5. Back"};

static int menu_select = 0;

// ==================== 图表数据缓冲区 ====================
#define CHART_WIDTH 240   // 屏幕宽度
#define CHART_HEIGHT 100  // 图表高度
#define CHART_HISTORY 240 // 历史数据点数

static uint16_t chart_r[CHART_HISTORY] = {0}; // 红色历史数据
static uint16_t chart_g[CHART_HISTORY] = {0}; // 绿色历史数据
static uint16_t chart_b[CHART_HISTORY] = {0}; // 蓝色历史数据
static int chart_index = 0;
static uint16_t chart_max_value = 65535; // 动态最大值

// ==================== GUI 绘制 ====================
static void draw_status_bar(void) {
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "TCS3472 Color Sensor");
}

static void draw_menu(void) {
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 140);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Function:");

  for (int i = 0; i < TCS3472_MENU_ITEMS; i++) {
    int y = 58 + i * 22; // 调整间距容纳6项

    if (i == menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 18);
      PD_SetFill(false);
      PD_SetColor(LCD_COLOR_WHITE);
    } else {
      PD_SetColor(LCD_COLOR_WHITE);
    }

    PD_DrawString(20, y, menus[i]);
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

// ==================== 图表绘制函数 ====================

// 绘制坐标轴和网格
static void draw_chart_axes(int x, int y, int width, int height,
                            uint16_t max_val) {
  PD_SetColor(LCD_COLOR_GRAY);

  // 边框
  PD_DrawRect(x, y, width, height);

  // 水平网格线 (4条)
  for (int i = 1; i <= 3; i++) {
    int line_y = y + (height * i / 4);
    PD_DrawLine(x, line_y, x + width, line_y);
  }

  // Y轴标签
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_WHITE);
  char label[8];

  snprintf(label, sizeof(label), "%d", max_val);
  PD_DrawString(x - 25, y - 4, label);

  snprintf(label, sizeof(label), "%d", max_val * 3 / 4);
  PD_DrawString(x - 25, y + height * 1 / 4 - 4, label);

  snprintf(label, sizeof(label), "%d", max_val / 2);
  PD_DrawString(x - 25, y + height * 2 / 4 - 4, label);

  snprintf(label, sizeof(label), "%d", max_val / 4);
  PD_DrawString(x - 25, y + height * 3 / 4 - 4, label);

  snprintf(label, sizeof(label), "0");
  PD_DrawString(x - 15, y + height - 4, label);
}

// 绘制单条数据线
static void draw_chart_line(uint16_t *data, int count, int x, int y, int width,
                            int height, uint16_t max_val, uint32_t color) {
  if (count < 2)
    return;

  PD_SetColor(color);

  // 计算绘制区域
  int start_x = x;
  int start_y = y + height;

  // 只绘制有效的点数
  int plot_count = (count < width) ? count : width;
  int step = 1;
  if (plot_count > width) {
    step = plot_count / width;
  }

  for (int i = 1; i < width && i < count; i++) {
    int idx_prev = (chart_index - 1 - i + count) % count;
    int idx_curr = (chart_index - i + count) % count;

    int x1 = x + width - i;
    int y1 = start_y - (int)((float)data[idx_prev] * height / max_val);
    int x2 = x + width - (i - 1);
    int y2 = start_y - (int)((float)data[idx_curr] * height / max_val);

    if (y1 >= y && y1 <= y + height && y2 >= y && y2 <= y + height) {
      PD_DrawLine(x1, y1, x2, y2);
    }
  }
}

// 绘制三条彩色数据线
static void draw_chart_all(int x, int y, int width, int height,
                           uint16_t max_val) {
  // 红色线
  draw_chart_line(chart_r, CHART_HISTORY, x, y, width, height, max_val,
                  LCD_COLOR_RED);
  // 绿色线
  draw_chart_line(chart_g, CHART_HISTORY, x, y, width, height, max_val,
                  LCD_COLOR_GREEN);
  // 蓝色线
  draw_chart_line(chart_b, CHART_HISTORY, x, y, width, height, max_val,
                  LCD_COLOR_BLUE);
}

// 更新图表数据
static void update_chart_data(uint16_t r, uint16_t g, uint16_t b) {
  chart_r[chart_index] = r;
  chart_g[chart_index] = g;
  chart_b[chart_index] = b;
  chart_index++;

  if (chart_index >= CHART_HISTORY) {
    chart_index = 0;
  }

  // 动态更新最大值（用于Y轴缩放）
  static uint16_t max_r = 0, max_g = 0, max_b = 0;
  if (r > max_r)
    max_r = r;
  if (g > max_g)
    max_g = g;
  if (b > max_b)
    max_b = b;

  uint16_t new_max = (max_r > max_g) ? max_r : max_g;
  new_max = (new_max > max_b) ? new_max : max_b;

  // 缓慢降低最大值（自适应缩放）
  if (new_max > chart_max_value) {
    chart_max_value = new_max;
  } else if (chart_max_value > 500 && new_max < chart_max_value / 2) {
    chart_max_value = chart_max_value * 3 / 4;
  }
  if (chart_max_value < 1000)
    chart_max_value = 1000;
}

// 重置图表
static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_r[i] = 0;
    chart_g[i] = 0;
    chart_b[i] = 0;
  }
  chart_index = 0;
  chart_max_value = 65535;
}

// ==================== 显示颜色块 ====================
static void show_color_block(uint16_t r, uint16_t g, uint16_t b) {
  uint32_t color = ((r >> 8) << 16) | ((g >> 8) << 8) | (b >> 8);

  PD_SetColor(color);
  PD_SetFill(true);
  PD_DrawRect(20, 140, 200, 60);
  PD_SetFill(false);

  // 边框
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(20, 140, 200, 60);
}

// ==================== 图表模式（A8 控制 LED 补光灯）====================

void tcs3472_chart_activity(void) {
  uint32_t last_update = HAL_GetTick();
  uint8_t last_a8 = 0, last_d0 = 0;
  bool led_state = false; // LED 补光灯状态
  uint32_t last_led_toggle = 0;

  printf("\r\n========== TCS3472 Chart Mode ==========\n");
  printf("Real-time RGB value chart\n");
  printf("A8: Toggle LED, D0: Reset chart, Enter: Exit\n");

  // 重置图表
  reset_chart();

  // 确保 LED 初始为关闭
  boardTCS3472.ledOff();

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    // A8: 切换 LED 补光灯（带防抖）
    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (current_a8 && !last_a8 && (HAL_GetTick() - last_led_toggle > 300)) {
      led_state = !led_state;
      if (led_state) {
        boardTCS3472.ledOn();
        printf("[CHART] LED ON\n");
      } else {
        boardTCS3472.ledOff();
        printf("[CHART] LED OFF\n");
      }
      last_led_toggle = HAL_GetTick();
    }
    last_a8 = current_a8;

    // D0: 重置图表数据
    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (current_d0 && !last_d0) {
      reset_chart();
      printf("[CHART] Chart reset\n");
    }
    last_d0 = current_d0;

    // 读取传感器数据
    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    // 更新图表数据
    update_chart_data(raw.red, raw.green, raw.blue);

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();

      // 标题
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_YELLOW);
      PD_DrawString(70, 25, "RGB Color Chart");

      // 绘制图表
      int chart_x = 15;
      int chart_y = 55;
      int chart_w = 210;
      int chart_h = 95;

      draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
      draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

      // 图例
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawRect(15, 160, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(28, 159, "Red");

      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawRect(75, 160, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(88, 159, "Green");

      PD_SetColor(LCD_COLOR_BLUE);
      PD_DrawRect(145, 160, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(158, 159, "Blue");

      // LED 状态指示
      if (led_state) {
        PD_SetColor(LCD_COLOR_YELLOW);
        PD_SetFill(true);
        PD_DrawRect(200, 158, 30, 12);
        PD_SetFill(false);
        PD_SetColor(LCD_COLOR_BLACK);
        PD_DrawString(205, 159, "LED");
      } else {
        PD_SetColor(LCD_COLOR_GRAY);
        PD_DrawRect(200, 158, 30, 12);
        PD_SetColor(LCD_COLOR_WHITE);
        PD_DrawString(205, 159, "LED");
      }

      // 当前 RGB 值
      PD_SetFont(FONT_ASCII_12);
      char dbg[48];
      PD_SetColor(LCD_COLOR_RED);
      snprintf(dbg, sizeof(dbg), "R:%4u", raw.red);
      PD_DrawString(15, 178, dbg);

      PD_SetColor(LCD_COLOR_GREEN);
      snprintf(dbg, sizeof(dbg), "G:%4u", raw.green);
      PD_DrawString(80, 178, dbg);

      PD_SetColor(LCD_COLOR_BLUE);
      snprintf(dbg, sizeof(dbg), "B:%4u", raw.blue);
      PD_DrawString(145, 178, dbg);

      // 色温预览
      PD_SetColor(LCD_COLOR_WHITE);
      char fstr[16];
      float_to_str(color.color_temp, fstr);
      snprintf(dbg, sizeof(dbg), "CCT: %s K", fstr);
      PD_DrawString(15, 195, dbg);

      float_to_str(color.lux, fstr);
      snprintf(dbg, sizeof(dbg), "Lux: %s", fstr);
      PD_DrawString(130, 195, dbg);

      // 提示（更新说明）
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 212, "A8:LED");
      PD_DrawString(55, 212, "D0:Rst");
      PD_DrawString(105, 212, "Enter:Exit");

      // Y轴范围指示
      snprintf(dbg, sizeof(dbg), "Max:%d", chart_max_value);
      PD_DrawString(170, 212, dbg);

      LCD_Flush();

      // 串口输出
      static uint32_t last_print = 0;
      if (HAL_GetTick() - last_print > 500) {
        last_print = HAL_GetTick();
        printf("[CHART] R=%4u, G=%4u, B=%4u, LED=%s\n", raw.red, raw.green,
               raw.blue, led_state ? "ON" : "OFF");
      }
    }

    HAL_Delay(30);
  }

  // 退出时关闭 LED
  boardTCS3472.ledOff();
  printf("========== Chart Mode Exit ==========\n");
}

// ==================== 功能实现 ====================
void tcs3472_read_activity(void) {
  char fstr[16];
  char dbg[64];

  if (!boardTCS3472.isInitialized()) {
    boardTCS3472.init();
    if (!boardTCS3472.isInitialized()) {
      PD_FillScreen(LCD_COLOR_BLACK);
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawString(20, 50, "TCS3472 Init Failed!");
      PD_DrawString(20, 70, "Check I2C connection");
      PD_DrawString(20, 100, "Press Enter to exit");
      LCD_Flush();

      while (1) {
        keyManager.btn_enter.tick();
        if (keyManager.btn_enter.getState() == KEY_PRESSED)
          break;
        HAL_Delay(50);
      }
      return;
    }
  }

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    PD_FillScreen(LCD_COLOR_BLACK);
    draw_status_bar();

    PD_SetFont(FONT_ASCII_12);

    // RGB 原始值
    PD_SetColor(LCD_COLOR_RED);
    sprintf(dbg, "R: %4u", raw.red);
    PD_DrawString(10, 35, dbg);

    PD_SetColor(LCD_COLOR_GREEN);
    sprintf(dbg, "G: %4u", raw.green);
    PD_DrawString(10, 50, dbg);

    PD_SetColor(LCD_COLOR_BLUE);
    sprintf(dbg, "B: %4u", raw.blue);
    PD_DrawString(10, 65, dbg);

    PD_SetColor(LCD_COLOR_WHITE);
    sprintf(dbg, "C: %4u", raw.clear);
    PD_DrawString(10, 80, dbg);

    // 色温和照度
    PD_SetColor(LCD_COLOR_CYAN);
    float_to_str(color.color_temp, fstr);
    sprintf(dbg, "CCT: %s K", fstr);
    PD_DrawString(10, 105, dbg);

    float_to_str(color.lux, fstr);
    sprintf(dbg, "Lux: %s lx", fstr);
    PD_DrawString(10, 120, dbg);

    // 颜色块
    show_color_block(raw.red, raw.green, raw.blue);

    // 提示
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(10, 210, "Enter: Exit");

    LCD_Flush();
    HAL_Delay(100);
  }
}

void tcs3472_color_demo(void) {
  if (!boardTCS3472.isInitialized()) {
    boardTCS3472.init();
  }

  printf("\r\n========== TCS3472 Color Demo ==========\r\n");
  printf("Place sensor on different colors\r\n");
  printf("Press Enter to exit\r\n");

  // 开启补光灯
  boardTCS3472.ledOn();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    // 清屏
    PD_FillScreen(LCD_COLOR_BLACK);

    // 大型颜色块（全屏下半部分）
    uint32_t display_color =
        ((raw.red >> 8) << 16) | ((raw.green >> 8) << 8) | (raw.blue >> 8);
    PD_SetColor(display_color);
    PD_SetFill(true);
    PD_DrawRect(0, 60, 240, 180);

    // 显示 RGB 数值
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(LCD_COLOR_WHITE);

    char dbg[32];
    sprintf(dbg, "R:%4u  G:%4u  B:%4u", raw.red, raw.green, raw.blue);
    PD_DrawString(10, 10, dbg);

    // 色温
    char fstr[16];
    float_to_str(color.color_temp, fstr);
    sprintf(dbg, "CCT: %s K", fstr);
    PD_DrawString(10, 35, dbg);

    // 提示退出
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(10, 220, "Enter: Exit");

    LCD_Flush();

    // 串口输出
    printf("RGB: %4u, %4u, %4u | CCT: %.0f K | Lux: %.1f\r\n", raw.red,
           raw.green, raw.blue, color.color_temp, color.lux);

    HAL_Delay(200);
  }

  boardTCS3472.ledOff();
  printf("========== Demo End ==========\r\n");
}

void tcs3472_cct_demo(void) {
  if (!boardTCS3472.isInitialized()) {
    boardTCS3472.init();
  }

  printf("\r\n========== TCS3472 CCT Demo ==========\r\n");
  printf("Measuring color temperature...\r\n");

  const uint32_t samples = 10;
  float cct_sum = 0, lux_sum = 0;
  uint16_t r_sum = 0, g_sum = 0, b_sum = 0;

  for (uint32_t i = 0; i < samples; i++) {
    TCS3472_RawData_t raw = boardTCS3472.readRaw();
    TCS3472_ColorData_t color = boardTCS3472.readColor();

    r_sum += raw.red;
    g_sum += raw.green;
    b_sum += raw.blue;
    cct_sum += color.color_temp;
    lux_sum += color.lux;

    HAL_Delay(50);
  }

  // 计算平均值
  r_sum /= samples;
  g_sum /= samples;
  b_sum /= samples;
  cct_sum /= samples;
  lux_sum /= samples;

  // 显示结果
  PD_FillScreen(LCD_COLOR_BLACK);
  draw_status_bar();

  PD_SetFont(FONT_ASCII_12);

  char dbg[64];
  char fstr[16];

  // RGB 平均值
  sprintf(dbg, "Avg R: %4u", r_sum);
  PD_SetColor(LCD_COLOR_RED);
  PD_DrawString(20, 40, dbg);

  sprintf(dbg, "Avg G: %4u", g_sum);
  PD_SetColor(LCD_COLOR_GREEN);
  PD_DrawString(20, 55, dbg);

  sprintf(dbg, "Avg B: %4u", b_sum);
  PD_SetColor(LCD_COLOR_BLUE);
  PD_DrawString(20, 70, dbg);

  // 色温（根据数值显示不同颜色）
  PD_SetColor(LCD_COLOR_WHITE);
  float_to_str(cct_sum, fstr);
  sprintf(dbg, "Color Temp: %s K", fstr);
  PD_DrawString(20, 100, dbg);

  float_to_str(lux_sum, fstr);
  sprintf(dbg, "Lux: %s lx", fstr);
  PD_DrawString(20, 115, dbg);

  // 色温描述
  PD_SetColor(LCD_COLOR_YELLOW);
  if (cct_sum < 3000) {
    PD_DrawString(20, 145, "Warm White (Incandescent)");
  } else if (cct_sum < 4500) {
    PD_DrawString(20, 145, "Neutral White");
  } else if (cct_sum < 5500) {
    PD_DrawString(20, 145, "Daylight");
  } else if (cct_sum < 7000) {
    PD_DrawString(20, 145, "Cool White");
  } else {
    PD_DrawString(20, 145, "Overcast/Cool");
  }

  PD_SetColor(LCD_COLOR_GRAY);
  PD_DrawString(20, 200, "Press Enter to exit");

  LCD_Flush();

  printf("CCT: %.0f K, Lux: %.1f lx, RGB: %u,%u,%u\r\n", cct_sum, lux_sum,
         r_sum, g_sum, b_sum);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED)
      break;
    HAL_Delay(50);
  }
}

// ==================== 主菜单 ====================
void tcs3472_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  menu_select = 0;

  // 初始化传感器
  boardTCS3472.init();

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== TCS3472 Activity Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    // 菜单导航
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= TCS3472_MENU_ITEMS) {
        menu_select = TCS3472_MENU_ITEMS - 1;
      }
      HAL_Delay(150);
    }

    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) {
        menu_select--;
      }
      HAL_Delay(150);
    }

    // 执行选中功能
    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      printf("Executing: %s\r\n", menus[menu_select]);

      switch (menu_select) {
      case 0:
        tcs3472_read_activity();
        break;
      case 1:
        tcs3472_color_demo();
        break;
      case 2:
        tcs3472_cct_demo();
        break;
        break;
      case 3:
        tcs3472_chart_activity(); // 新增图表模式
        break;
      case 4:
        printf("Exit TCS3472 Activity\r\n");
        return;
      }

      // 刷新菜单界面
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter_state;

    // 定时刷新显示
    if (HAL_GetTick() - last_update > 100) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();
      draw_menu();
      draw_bottom_bar();
      LCD_Flush();
    }

    HAL_Delay(20);
  }
}