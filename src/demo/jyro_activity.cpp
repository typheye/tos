#include "include/jyro_activity.hpp"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include <cstdint>
#include <math.h>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern LCD boardLCD;
extern JY901S boardJY901S;
extern KeyManager keyManager;

static int menu_select = 0;

// ==================== 菜单 GUI 绘制 ====================
static void draw_status_bar(void) {
  PD_SetColor(LCD_COLOR_BLUE);
  PD_SetFill(true);
  PD_DrawRect(0, 0, 240, 22);
  PD_SetFill(false);

  PD_SetFont(FONT_ASCII_16);
  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawString(10, 4, "JY901S 9-Axis Sensor");
}

static void draw_menu(void) {
  const char *menus[] = {"1. 3D Cube Display", "2. Text Display",
                         "3. Chart Display", "4. Back"};

  PD_SetColor(LCD_COLOR_WHITE);
  PD_DrawRect(10, 32, 220, 140);

  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LCD_COLOR_YELLOW);
  PD_DrawString(15, 38, "Select Function:");

  for (int i = 0; i < 4; i++) {
    int y = 58 + i * 24;

    if (i == menu_select) {
      PD_SetColor(LCD_COLOR_BLUE);
      PD_SetFill(true);
      PD_DrawRect(15, y - 2, 210, 20);
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

// ==================== 图表数据缓冲区 ====================
#define CHART_WIDTH 240
#define CHART_HEIGHT 100
#define CHART_HISTORY 240

static CCMRAM float chart_data[3][CHART_HISTORY];
static CCMRAM int chart_index = 0;
static CCMRAM float chart_max_value = 10.0f;
static CCMRAM int chart_param_group = 0;

// 参数组定义
typedef struct {
  const char *name;
  const char *unit;
  float (*getValue)(JY901S_Data_t *);
} ChartParam_t;

// 加速度参数组
static float getAccX(JY901S_Data_t *d) { return d->acc_x; }
static float getAccY(JY901S_Data_t *d) { return d->acc_y; }
static float getAccZ(JY901S_Data_t *d) { return d->acc_z; }

// 角速度参数组
static float getGyrX(JY901S_Data_t *d) { return d->gyro_x; }
static float getGyrY(JY901S_Data_t *d) { return d->gyro_y; }
static float getGyrZ(JY901S_Data_t *d) { return d->gyro_z; }

// 角度参数组
static float getRoll(JY901S_Data_t *d) {
  static float roll = 0;
  // 简单滤波，实际可从传感器读取
  return roll;
}
static float getPitch(JY901S_Data_t *d) {
  static float pitch = 0;
  return pitch;
}
static float getYaw(JY901S_Data_t *d) {
  static float yaw = 0;
  return yaw;
}

static const ChartParam_t param_groups[3][3] = {
    // 加速度组
    {{"AccX", "g", getAccX}, {"AccY", "g", getAccY}, {"AccZ", "g", getAccZ}},
    // 角速度组
    {{"GyrX", "deg/s", getGyrX},
     {"GyrY", "deg/s", getGyrY},
     {"GyrZ", "deg/s", getGyrZ}},
    // 角度组（需要积分计算）
    {{"Roll", "deg", getRoll},
     {"Pitch", "deg", getPitch},
     {"Yaw", "deg", getYaw}}};

static const char *group_names[3] = {"Acceleration", "Angular Velocity",
                                     "Euler Angles"};
static const uint32_t group_colors[3] = {LCD_COLOR_GREEN, LCD_COLOR_ORANGE,
                                         LCD_COLOR_YELLOW};

// 更新图表数据
static void update_chart_data(float v0, float v1, float v2) {
  chart_data[0][chart_index] = v0;
  chart_data[1][chart_index] = v1;
  chart_data[2][chart_index] = v2;
  chart_index++;

  if (chart_index >= CHART_HISTORY) {
    chart_index = 0;
  }

  // 动态更新最大值
  float max_val = fabsf(v0);
  if (fabsf(v1) > max_val)
    max_val = fabsf(v1);
  if (fabsf(v2) > max_val)
    max_val = fabsf(v2);

  if (max_val > chart_max_value) {
    chart_max_value = max_val * 1.1f;
  } else if (chart_max_value > 5.0f && max_val < chart_max_value / 2) {
    chart_max_value = chart_max_value * 0.9f;
  }
  if (chart_max_value < 0.1f)
    chart_max_value = 1.0f;
}

// 重置图表
static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_data[0][i] = 0;
    chart_data[1][i] = 0;
    chart_data[2][i] = 0;
  }
  chart_index = 0;
  chart_max_value = 10.0f;
}

// 绘制坐标轴和网格
static void draw_chart_axes(int x, int y, int width, int height,
                            float max_val) {
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
  char label[16];

  snprintf(label, sizeof(label), "%.0f", max_val);
  PD_DrawString(x - 25, y - 4, label);

  snprintf(label, sizeof(label), "%.0f", max_val * 0.5f);
  PD_DrawString(x - 25, y + height / 2 - 4, label);

  snprintf(label, sizeof(label), "%.0f", -max_val * 0.5f);
  PD_DrawString(x - 25, y + height * 3 / 4 - 4, label);

  snprintf(label, sizeof(label), "%.0f", -max_val);
  PD_DrawString(x - 25, y + height - 4, label);
}

// 绘制单条数据线
static void draw_chart_line(float *data, int count, int x, int y, int width,
                            int height, float max_val, uint32_t color) {
  if (count < 2)
    return;

  PD_SetColor(color);

  int center_y = y + height / 2;

  for (int i = 1; i < width && i < count; i++) {
    int idx_prev = (chart_index - 1 - i + count) % count;
    int idx_curr = (chart_index - i + count) % count;

    float val_prev = data[idx_prev];
    float val_curr = data[idx_curr];

    // 限制范围
    if (val_prev > max_val)
      val_prev = max_val;
    if (val_prev < -max_val)
      val_prev = -max_val;
    if (val_curr > max_val)
      val_curr = max_val;
    if (val_curr < -max_val)
      val_curr = -max_val;

    int y1 = center_y - (int)(val_prev * height / (2 * max_val));
    int y2 = center_y - (int)(val_curr * height / (2 * max_val));
    int x1 = x + width - i;
    int x2 = x + width - (i - 1);

    if (y1 >= y && y1 <= y + height && y2 >= y && y2 <= y + height) {
      PD_DrawLine(x1, y1, x2, y2);
    }
  }
}

// 绘制三条数据线
static void draw_chart_all(int x, int y, int width, int height, float max_val) {
  uint32_t colors[3] = {LCD_COLOR_RED, LCD_COLOR_GREEN, LCD_COLOR_BLUE};
  for (int i = 0; i < 3; i++) {
    draw_chart_line(chart_data[i], CHART_HISTORY, x, y, width, height, max_val,
                    colors[i]);
  }
}

// ==================== 3D 立方体显示（原功能）====================
void jyro_cube_activity(void) {
  PD_Init();
  gyro_cube_init(120, 120, 80);

  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetBgColor(LCD_COLOR_BLACK);

#define SMOOTH_WINDOW 3
  float gyro_x_history[SMOOTH_WINDOW] = {0};
  float gyro_y_history[SMOOTH_WINDOW] = {0};
  float gyro_z_history[SMOOTH_WINDOW] = {0};
  int history_index = 0;

  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = HAL_GetTick();

  int16_t text_x = 5;
  int16_t line_height = 12;
  int16_t start_y = 5;
  char fstr[16];
  char display_str[32];

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    JY901S_Data_t data = boardJY901S.readData();

    gyro_x_history[history_index] = data.gyro_x;
    gyro_y_history[history_index] = data.gyro_y;
    gyro_z_history[history_index] = data.gyro_z;

    float smooth_gyro_x = 0, smooth_gyro_y = 0, smooth_gyro_z = 0;
    for (int i = 0; i < SMOOTH_WINDOW; i++) {
      smooth_gyro_x += gyro_x_history[i];
      smooth_gyro_y += gyro_y_history[i];
      smooth_gyro_z += gyro_z_history[i];
    }
    smooth_gyro_x /= SMOOTH_WINDOW;
    smooth_gyro_y /= SMOOTH_WINDOW;
    smooth_gyro_z /= SMOOTH_WINDOW;

    history_index = (history_index + 1) % SMOOTH_WINDOW;

    roll += smooth_gyro_x * dt * 0.0174533f;
    pitch += smooth_gyro_y * dt * 0.0174533f;
    yaw += smooth_gyro_z * dt * 0.0174533f;

    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));

    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    if (roll > 3.14159f)
      roll -= 6.28318f;
    if (roll < -3.14159f)
      roll += 6.28318f;
    if (pitch > 3.14159f)
      pitch -= 6.28318f;
    if (pitch < -3.14159f)
      pitch += 6.28318f;
    if (yaw > 3.14159f)
      yaw -= 6.28318f;
    if (yaw < -3.14159f)
      yaw += 6.28318f;

    PD_FillScreen(LCD_COLOR_BLACK);
    gyro_cube_draw(roll, pitch, yaw);

    // 显示数据
    PD_SetFont(FONT_ASCII_12);

    PD_SetColor(LCD_COLOR_YELLOW);
    float_to_str(roll * 57.29578f, fstr);
    sprintf(display_str, "Roll: %s", fstr);
    PD_DrawString(text_x, start_y, display_str);

    float_to_str(pitch * 57.29578f, fstr);
    sprintf(display_str, "Pitch: %s", fstr);
    PD_DrawString(text_x, start_y + line_height, display_str);

    float_to_str(yaw * 57.29578f, fstr);
    sprintf(display_str, "Yaw: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 2, display_str);

    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(text_x, 220, "Enter: Exit");

    LCD_Flush();
  }
}

// ==================== 纯文本显示（所有参数）====================
void jyro_text_activity(void) {
  int16_t text_x = 5;
  int16_t line_height = 14;
  int16_t start_y = 30;
  char fstr[16];
  char display_str[32];

  printf("\r\n========== JY901S Text Display ==========\r\n");
  printf("Press Enter to exit\n");

  PD_FillScreen(LCD_COLOR_BLACK);
  draw_status_bar();

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    JY901S_Data_t data = boardJY901S.readData();

    // 使用互补滤波计算角度
    static float roll = 0, pitch = 0, yaw = 0;
    static uint32_t last_time = 0;
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    roll += data.gyro_x * dt * 0.0174533f;
    pitch += data.gyro_y * dt * 0.0174533f;
    yaw += data.gyro_z * dt * 0.0174533f;

    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));

    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    PD_FillScreen(LCD_COLOR_BLACK);
    draw_status_bar();

    PD_SetFont(FONT_ASCII_12);

    // 标题
    PD_SetColor(LCD_COLOR_YELLOW);
    PD_DrawString(text_x, start_y, "=== Sensor Data ===");

    int y = start_y + line_height;

    // 加速度
    PD_SetColor(LCD_COLOR_GREEN);
    float_to_str(data.acc_x, fstr);
    sprintf(display_str, "AccX: %s g", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(data.acc_y, fstr);
    sprintf(display_str, "AccY: %s g", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(data.acc_z, fstr);
    sprintf(display_str, "AccZ: %s g", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height + 5;

    // 角速度
    PD_SetColor(LCD_COLOR_ORANGE);
    float_to_str(data.gyro_x, fstr);
    sprintf(display_str, "GyrX: %s deg/s", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(data.gyro_y, fstr);
    sprintf(display_str, "GyrY: %s deg/s", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(data.gyro_z, fstr);
    sprintf(display_str, "GyrZ: %s deg/s", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height + 5;

    // 角度
    PD_SetColor(LCD_COLOR_CYAN);
    float_to_str(roll * 57.29578f, fstr);
    sprintf(display_str, "Roll: %s deg", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(pitch * 57.29578f, fstr);
    sprintf(display_str, "Pitch: %s deg", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height;

    float_to_str(yaw * 57.29578f, fstr);
    sprintf(display_str, "Yaw: %s deg", fstr);
    PD_DrawString(text_x, y, display_str);
    y += line_height + 5;

    // 温度
    PD_SetColor(LCD_COLOR_WHITE);
    float_to_str(data.temperature, fstr);
    sprintf(display_str, "Temp: %s C", fstr);
    PD_DrawString(text_x, y, display_str);

    // 提示
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(text_x, 220, "Enter: Exit");

    LCD_Flush();
    HAL_Delay(50);
  }
}

// ==================== 图表显示模式 ====================
void jyro_chart_activity(void) {
  uint32_t last_update = HAL_GetTick();
  uint8_t last_a8 = 0, last_d0 = 0;
  uint32_t last_param_switch = 0;

  // 姿态角度积分变量
  static float roll = 0, pitch = 0, yaw = 0;
  static uint32_t last_time = 0;

  printf("\r\n========== JY901S Chart Mode ==========\n");
  printf("A8: Switch parameter group, D0: Reset chart, Enter: Exit\n");

  reset_chart();
  chart_param_group = 0;

  PD_FillScreen(LCD_COLOR_BLACK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break;
    }

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    // A8: 切换参数组
    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (current_a8 && !last_a8 && (HAL_GetTick() - last_param_switch > 300)) {
      chart_param_group = (chart_param_group + 1) % 3;
      reset_chart();
      last_param_switch = HAL_GetTick();
      printf("[CHART] Switched to: %s\n", group_names[chart_param_group]);
    }
    last_a8 = current_a8;

    // D0: 重置图表
    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (current_d0 && !last_d0) {
      reset_chart();
      printf("[CHART] Chart reset\n");
    }
    last_d0 = current_d0;

    // 读取传感器数据
    JY901S_Data_t data = boardJY901S.readData();

    // 计算角度（互补滤波）
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    roll += data.gyro_x * dt * 0.0174533f;
    pitch += data.gyro_y * dt * 0.0174533f;
    yaw += data.gyro_z * dt * 0.0174533f;

    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));

    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    // 获取当前参数组的值
    float val0, val1, val2;
    switch (chart_param_group) {
    case 0: // 加速度
      val0 = data.acc_x;
      val1 = data.acc_y;
      val2 = data.acc_z;
      break;
    case 1: // 角速度
      val0 = data.gyro_x;
      val1 = data.gyro_y;
      val2 = data.gyro_z;
      break;
    case 2: // 角度
      val0 = roll * 57.29578f;
      val1 = pitch * 57.29578f;
      val2 = yaw * 57.29578f;
      break;
    default:
      val0 = val1 = val2 = 0;
    }

    // 更新图表
    update_chart_data(val0, val1, val2);

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();

      PD_FillScreen(LCD_COLOR_BLACK);
      draw_status_bar();

      // 标题
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(group_colors[chart_param_group]);
      char title[32];
      snprintf(title, sizeof(title), "Chart: %s",
               group_names[chart_param_group]);
      PD_DrawString((240 - strlen(title) * 6) / 2, 25, title);

      // 绘制图表
      int chart_x = 15;
      int chart_y = 55;
      int chart_w = 210;
      int chart_h = 100;

      draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
      draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

      // 图例
      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LCD_COLOR_RED);
      PD_DrawRect(15, 168, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(28, 167, param_groups[chart_param_group][0].name);

      PD_SetColor(LCD_COLOR_GREEN);
      PD_DrawRect(90, 168, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(103, 167, param_groups[chart_param_group][1].name);

      PD_SetColor(LCD_COLOR_BLUE);
      PD_DrawRect(165, 168, 10, 8);
      PD_SetColor(LCD_COLOR_WHITE);
      PD_DrawString(178, 167, param_groups[chart_param_group][2].name);

      // 当前数值
      PD_SetFont(FONT_ASCII_12);
      char fstr[16];
      char dbg[48];

      PD_SetColor(LCD_COLOR_RED);
      float_to_str(val0, fstr);
      snprintf(dbg, sizeof(dbg), "%s: %s %s",
               param_groups[chart_param_group][0].name, fstr,
               param_groups[chart_param_group][0].unit);
      PD_DrawString(15, 185, dbg);

      PD_SetColor(LCD_COLOR_GREEN);
      float_to_str(val1, fstr);
      snprintf(dbg, sizeof(dbg), "%s: %s %s",
               param_groups[chart_param_group][1].name, fstr,
               param_groups[chart_param_group][1].unit);
      PD_DrawString(15, 198, dbg);

      PD_SetColor(LCD_COLOR_BLUE);
      float_to_str(val2, fstr);
      snprintf(dbg, sizeof(dbg), "%s: %s %s",
               param_groups[chart_param_group][2].name, fstr,
               param_groups[chart_param_group][2].unit);
      PD_DrawString(15, 211, dbg);

      // 提示
      PD_SetColor(LCD_COLOR_GRAY);
      PD_DrawString(10, 222, "A8:Switch");
      PD_DrawString(80, 222, "D0:Reset");
      PD_DrawString(140, 222, "Enter:Exit");

      LCD_Flush();
    }

    HAL_Delay(30);
  }

  printf("========== Chart Mode Exit ==========\n");
}

// ==================== 主菜单 ====================
void jyro_activity(void) {
  uint8_t last_enter_state = 0;
  uint32_t last_update = HAL_GetTick();

  PD_Init();
  menu_select = 0;

  PD_FillScreen(LCD_COLOR_BLACK);

  printf("\r\n========== JY901S Activity Started ==========\r\n");

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= 4)
        menu_select = 3;
      HAL_Delay(150);
    }

    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0)
        menu_select--;
      HAL_Delay(150);
    }

    uint8_t current_enter_state =
        (keyManager.btn_enter.getState() == KEY_PRESSED) ? 1 : 0;
    if (current_enter_state == 1 && last_enter_state == 0) {
      switch (menu_select) {
      case 0:
        jyro_cube_activity();
        break;
      case 1:
        jyro_text_activity();
        break;
      case 2:
        jyro_chart_activity();
        break;
      case 3:
        printf("Exit JY901S Activity\n");
        return;
      }
      PD_FillScreen(LCD_COLOR_BLACK);
    }
    last_enter_state = current_enter_state;

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