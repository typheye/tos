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

#define JYRO_MENU_ITEMS 4
static const char *jyro_menus[JYRO_MENU_ITEMS] = {
    "01 3D Cube", "02 Text", "03 Chart", "04 Back"};

static int menu_select = 0;

static void bar(const char *t) {
  PD_DrawFrame();
  PD_SetFont(FONT_ASCII_16); PD_SetColor(TOS_ACCENT);
  PD_DrawString(22, 5, t);
}
static void bbar(const char *l, const char *m, const char *r) {
  PD_DrawFooterCenter(l, m, r);
}
static void menu_cards(int sel) {
  PD_SetFont(FONT_ASCII_16);
  for (int i = 0; i < JYRO_MENU_ITEMS; i++) {
    int cy = 28 + i * 25;
    if (i == sel) {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
      PD_SetColor(TOS_TEXT);
    } else {
      PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
      PD_SetColor(TOS_TEXT_SEC);
    }
    PD_DrawString(26, cy + 2, jyro_menus[i]);
  }
}

// 图表数据
#define CHART_HISTORY 240
static CCMRAM float chart_data[3][CHART_HISTORY];
static CCMRAM int chart_index = 0;
static CCMRAM float chart_max_value = 10.0f;
static CCMRAM int chart_param_group = 0;

static const char *group_names[3] = {"Acceleration", "Angular Velocity",
                                     "Euler Angles"};

static void update_chart_data(float v0, float v1, float v2) {
  chart_data[0][chart_index] = v0;
  chart_data[1][chart_index] = v1;
  chart_data[2][chart_index] = v2;
  chart_index++;
  if (chart_index >= CHART_HISTORY) chart_index = 0;

  float max_val = fabsf(v0);
  if (fabsf(v1) > max_val) max_val = fabsf(v1);
  if (fabsf(v2) > max_val) max_val = fabsf(v2);

  if (max_val > chart_max_value)
    chart_max_value = max_val * 1.1f;
  else if (chart_max_value > 5.0f && max_val < chart_max_value / 2)
    chart_max_value = chart_max_value * 0.9f;
  if (chart_max_value < 0.1f) chart_max_value = 1.0f;
}

static void reset_chart(void) {
  for (int i = 0; i < CHART_HISTORY; i++) {
    chart_data[0][i] = 0; chart_data[1][i] = 0; chart_data[2][i] = 0;
  }
  chart_index = 0;
  chart_max_value = 10.0f;
}

static void draw_chart_axes(int x, int y, int width, int height, float max_val) {
  PD_SetColor(LV_BORDER);
  PD_DrawRect(x, y, width, height);
  for (int i = 1; i <= 3; i++) {
    int line_y = y + (height * i / 4);
    PD_SetColor(LV_BORDER);
    PD_DrawLine(x, line_y, x + width, line_y);
  }
  PD_SetFont(FONT_ASCII_12);
  PD_SetColor(LV_TEXT_HINT);
  char label[16];
  snprintf(label, sizeof(label), "%.0f", max_val);
  PD_DrawString(x - 25, y - 4, label);
  snprintf(label, sizeof(label), "%.0f", -max_val);
  PD_DrawString(x - 25, y + height - 4, label);
}

static void draw_chart_line(float *data, int count, int x, int y, int width,
                            int height, float max_val, uint32_t color) {
  if (count < 2) return;
  PD_SetColor(color);
  int center_y = y + height / 2;
  for (int i = 1; i < width && i < count; i++) {
    int idx_prev = (chart_index - 1 - i + count) % count;
    int idx_curr = (chart_index - i + count) % count;
    float val_prev = data[idx_prev];
    float val_curr = data[idx_curr];
    if (val_prev > max_val) val_prev = max_val;
    if (val_prev < -max_val) val_prev = -max_val;
    if (val_curr > max_val) val_curr = max_val;
    if (val_curr < -max_val) val_curr = -max_val;
    int y1 = center_y - (int)(val_prev * height / (2 * max_val));
    int y2 = center_y - (int)(val_curr * height / (2 * max_val));
    int x1 = x + width - i;
    int x2 = x + width - (i - 1);
    if (y1 >= y && y1 <= y + height && y2 >= y && y2 <= y + height) {
      PD_DrawLine(x1, y1, x2, y2);
    }
  }
}

static void draw_chart_all(int x, int y, int width, int height, float max_val) {
  uint32_t colors[3] = {LV_ERROR, LV_SUCCESS, LV_PRIMARY};
  for (int i = 0; i < 3; i++) {
    draw_chart_line(chart_data[i], CHART_HISTORY, x, y, width, height, max_val, colors[i]);
  }
}

// 3D 立方体显示
void jyro_cube_activity(void) {
  PD_Init();
  gyro_cube_init(120, 120, 80);
  PD_SetColor(LV_TEXT_PRIMARY);
  PD_SetBgColor(LV_BG_DARK);

#define SMOOTH_WINDOW 3
  float gyro_x_history[SMOOTH_WINDOW] = {0};
  float gyro_y_history[SMOOTH_WINDOW] = {0};
  float gyro_z_history[SMOOTH_WINDOW] = {0};
  int history_index = 0;
  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = HAL_GetTick();
  char fstr[16];
  char display_str[32];

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f) dt = 0.02f;
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
    float pitch_acc = atan2(-data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    if (roll > 3.14159f) roll -= 6.28318f;
    if (roll < -3.14159f) roll += 6.28318f;
    if (pitch > 3.14159f) pitch -= 6.28318f;
    if (pitch < -3.14159f) pitch += 6.28318f;
    if (yaw > 3.14159f) yaw -= 6.28318f;
    if (yaw < -3.14159f) yaw += 6.28318f;

    PD_FillScreen(LV_BG_DARK);
    gyro_cube_draw(roll, pitch, yaw);

    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(LV_ACCENT);
    float_to_str(roll * 57.29578f, fstr);
    sprintf(display_str, "Roll: %s", fstr);
    PD_DrawString(5, 5, display_str);

    float_to_str(pitch * 57.29578f, fstr);
    sprintf(display_str, "Pitch:%s", fstr);
    PD_DrawString(5, 18, display_str);

    float_to_str(yaw * 57.29578f, fstr);
    sprintf(display_str, "Yaw:  %s", fstr);
    PD_DrawString(5, 31, display_str);

    bbar("EXIT", NULL, NULL);
    LCD_Flush();
  }
}

// 文本显示
void jyro_text_activity(void) {
  char fstr[16];
  char display_str[32];

  printf("\r\n========== JY901S Text Display ==========\r\n");
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    JY901S_Data_t data = boardJY901S.readData();

    static float roll = 0, pitch = 0, yaw = 0;
    static uint32_t last_time = 0;
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f) dt = 0.02f;
    last_time = now;

    roll += data.gyro_x * dt * 0.0174533f;
    pitch += data.gyro_y * dt * 0.0174533f;
    yaw += data.gyro_z * dt * 0.0174533f;
    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(-data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    PD_FillScreen(LV_BG_DARK);
    bar("JY901S Sensor");

    PD_DrawAngledCard(8, 44, 224, 164, 6, TOS_CARD_BG);
    PD_SetFont(FONT_ASCII_16);

    int y = 54;
    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "AccX"); y += 20;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.acc_x, fstr);
    sprintf(display_str, "%s g", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "AccY"); y += 20;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.acc_y, fstr);
    sprintf(display_str, "%s g", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "AccZ"); y += 26;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.acc_z, fstr);
    sprintf(display_str, "%s g", fstr);
    PD_DrawString(100, y - 26, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "GyrX"); y += 20;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.gyro_x, fstr);
    sprintf(display_str, "%s d/s", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "GyrY"); y += 20;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.gyro_y, fstr);
    sprintf(display_str, "%s d/s", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "GyrZ"); y += 26;
    PD_SetColor(LV_TEXT_PRIMARY);
    float_to_str(data.gyro_z, fstr);
    sprintf(display_str, "%s d/s", fstr);
    PD_DrawString(100, y - 26, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "Roll"); y += 20;
    PD_SetColor(LV_ACCENT);
    float_to_str(roll * 57.29578f, fstr);
    sprintf(display_str, "%s deg", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "Pitch"); y += 20;
    PD_SetColor(LV_ACCENT);
    float_to_str(pitch * 57.29578f, fstr);
    sprintf(display_str, "%s deg", fstr);
    PD_DrawString(100, y - 20, display_str);

    PD_SetColor(LV_TEXT_HINT);
    PD_DrawString(16, y, "Yaw");
    PD_SetColor(LV_ACCENT);
    float_to_str(yaw * 57.29578f, fstr);
    sprintf(display_str, "%s deg", fstr);
    PD_DrawString(100, y, display_str);

    bbar("EXIT", NULL, NULL);
    LCD_Flush();
    HAL_Delay(50);
  }
}

// 图表模式
void jyro_chart_activity(void) {
  uint32_t last_update = HAL_GetTick();
  uint8_t last_a8 = 0, last_d0 = 0;
  uint32_t last_param_switch = 0;
  static float roll = 0, pitch = 0, yaw = 0;
  static uint32_t last_time = 0;

  reset_chart();
  chart_param_group = 0;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) break;

    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();

    uint8_t current_a8 = (keyManager.collision_A8.getState() == KEY_PRESSED);
    if (current_a8 && !last_a8 && (HAL_GetTick() - last_param_switch > 300)) {
      chart_param_group = (chart_param_group + 1) % 3;
      reset_chart();
      last_param_switch = HAL_GetTick();
    }
    last_a8 = current_a8;

    uint8_t current_d0 = (keyManager.collision_D0.getState() == KEY_PRESSED);
    if (current_d0 && !last_d0) reset_chart();
    last_d0 = current_d0;

    JY901S_Data_t data = boardJY901S.readData();
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f) dt = 0.02f;
    last_time = now;

    roll += data.gyro_x * dt * 0.0174533f;
    pitch += data.gyro_y * dt * 0.0174533f;
    yaw += data.gyro_z * dt * 0.0174533f;
    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(-data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    float val0, val1, val2;
    switch (chart_param_group) {
    case 0: val0 = data.acc_x; val1 = data.acc_y; val2 = data.acc_z; break;
    case 1: val0 = data.gyro_x; val1 = data.gyro_y; val2 = data.gyro_z; break;
    case 2: val0 = roll * 57.29578f; val1 = pitch * 57.29578f; val2 = yaw * 57.29578f; break;
    default: val0 = val1 = val2 = 0;
    }
    update_chart_data(val0, val1, val2);

    if (HAL_GetTick() - last_update > 50) {
      last_update = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("JY901S Chart");

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LV_ACCENT);
      char title[32];
      snprintf(title, sizeof(title), "Chart: %s", group_names[chart_param_group]);
      PD_DrawString(16, 44, title);

      int chart_x = 10, chart_y = 58, chart_w = 220, chart_h = 90;
      draw_chart_axes(chart_x, chart_y, chart_w, chart_h, chart_max_value);
      draw_chart_all(chart_x, chart_y, chart_w, chart_h, chart_max_value);

      PD_SetFont(FONT_ASCII_12);
      PD_SetColor(LV_ERROR); PD_DrawRect(10, 156, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY);
      PD_DrawString(23, 155, chart_param_group==0?"AccX":chart_param_group==1?"GyrX":"Roll");
      PD_SetColor(LV_SUCCESS); PD_DrawRect(80, 156, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY);
      PD_DrawString(93, 155, chart_param_group==0?"AccY":chart_param_group==1?"GyrY":"Pitch");
      PD_SetColor(LV_PRIMARY); PD_DrawRect(150, 156, 10, 8);
      PD_SetColor(LV_TEXT_PRIMARY);
      PD_DrawString(163, 155, chart_param_group==0?"AccZ":chart_param_group==1?"GyrZ":"Yaw");

      char fstr[16]; char dbg[48];
      PD_SetColor(LV_ERROR);
      float_to_str(val0, fstr); snprintf(dbg, sizeof(dbg), "%s", fstr);
      PD_DrawString(10, 175, dbg);
      PD_SetColor(LV_SUCCESS);
      float_to_str(val1, fstr); snprintf(dbg, sizeof(dbg), "%s", fstr);
      PD_DrawString(80, 175, dbg);
      PD_SetColor(LV_PRIMARY);
      float_to_str(val2, fstr); snprintf(dbg, sizeof(dbg), "%s", fstr);
      PD_DrawString(150, 175, dbg);

      bbar("EXIT", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(30);
  }
}

// 主菜单
void jyro_activity(void) {
  uint8_t le = 0;
  uint32_t lu = HAL_GetTick();

  PD_Init();
  menu_select = 0;
  PD_FillScreen(LV_BG_DARK);

  while (1) {
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();

    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      menu_select++;
      if (menu_select >= JYRO_MENU_ITEMS) menu_select = JYRO_MENU_ITEMS - 1;
      HAL_Delay(150);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      if (menu_select > 0) menu_select--;
      HAL_Delay(150);
    }

    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      switch (menu_select) {
      case 0: jyro_cube_activity(); break;
      case 1: jyro_text_activity(); break;
      case 2: jyro_chart_activity(); break;
      case 3: return;
      }
      PD_FillScreen(LV_BG_DARK);
    }
    le = ce;

    if (HAL_GetTick() - lu > 100) {
      lu = HAL_GetTick();
      PD_FillScreen(LV_BG_DARK);
      bar("04");
      PD_SetFont(FONT_ASCII_12); PD_SetColor(LV_TEXT_HINT);
      PD_DrawString(16, 24, "Select Function:");
      menu_cards(menu_select);
      bbar("ENTER", NULL, "UP/DOWN");
      LCD_Flush();
    }
    HAL_Delay(20);
  }
}
