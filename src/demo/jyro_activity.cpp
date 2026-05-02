#include "include/jyro_activity.hpp"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "include/lib3dgyro.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include <cstdint>
#include <math.h>

extern LCD boardLCD;
extern JY901S boardJY901S;
extern KeyManager keyManager;

void gyro_cube_activity(void) {
  // 初始化 PD 图形库（会自动获取帧缓冲区）
  PD_Init();
  gyro_cube_init(120, 120, 80);

  PD_SetColor(LCD_COLOR_WHITE);
  PD_SetBgColor(LCD_COLOR_BLACK);

// 平滑滤波变量
#define SMOOTH_WINDOW 3
  float gyro_x_history[SMOOTH_WINDOW] = {0};
  float gyro_y_history[SMOOTH_WINDOW] = {0};
  float gyro_z_history[SMOOTH_WINDOW] = {0};
  int history_index = 0;

  // 姿态角度（弧度）
  float roll = 0, pitch = 0, yaw = 0;
  uint32_t last_time = HAL_GetTick();

  while (1) {
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    if (dt > 0.05f)
      dt = 0.02f;
    last_time = now;

    JY901S_Data_t data = boardJY901S.readData();

    // 更新陀螺仪历史数据
    gyro_x_history[history_index] = data.gyro_x;
    gyro_y_history[history_index] = data.gyro_y;
    gyro_z_history[history_index] = data.gyro_z;

    // 计算平滑陀螺仪值
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

    // 陀螺仪积分更新姿态
    roll += smooth_gyro_x * dt * 0.0174533f;
    pitch += smooth_gyro_y * dt * 0.0174533f;
    yaw += smooth_gyro_z * dt * 0.0174533f;

    // 使用加速度计修正
    float roll_acc = atan2(data.acc_y, data.acc_z);
    float pitch_acc = atan2(
        -data.acc_x, sqrt(data.acc_y * data.acc_y + data.acc_z * data.acc_z));

    // 互补滤波
    roll = roll * 0.98f + roll_acc * 0.02f;
    pitch = pitch * 0.98f + pitch_acc * 0.02f;

    // 限制范围
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

    // 清屏
    PD_FillScreen(LCD_COLOR_BLACK);

    // 绘制立方体
    gyro_cube_draw(roll, pitch, yaw);

    // 刷新到屏幕
    LCD_Flush();
  }
}

void gyro_cube_activity_with_exit(void) {
  // 初始化 PD 图形库（会自动获取帧缓冲区）
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

  // 文字显示位置
  int16_t text_x = 5;
  int16_t line_height = 12; // FONT_ASCII_12 高度
  int16_t start_y = 5;

  // 用于存储转换后的字符串
  char fstr[16];
  char display_str[32];

  while (1) {
    // 检查退出条件（标准按键 btn_enter 按下）
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      break; // 退出循环，返回菜单
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

    // 清屏
    PD_FillScreen(LCD_COLOR_BLACK);

    // 绘制立方体
    gyro_cube_draw(roll, pitch, yaw);

    // ==================== 显示九轴数据 ====================
    PD_SetFont(FONT_ASCII_12);
    PD_SetColor(LCD_COLOR_CYAN);

    // 角度数据 (Roll, Pitch, Yaw)
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

    // 加速度数据
    PD_SetColor(LCD_COLOR_GREEN);
    float_to_str(data.acc_x, fstr);
    sprintf(display_str, "AccX: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 4, display_str);

    float_to_str(data.acc_y, fstr);
    sprintf(display_str, "AccY: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 5, display_str);

    float_to_str(data.acc_z, fstr);
    sprintf(display_str, "AccZ: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 6, display_str);

    // 角速度数据
    PD_SetColor(LCD_COLOR_ORANGE);
    float_to_str(data.gyro_x, fstr);
    sprintf(display_str, "GyrX: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 8, display_str);

    float_to_str(data.gyro_y, fstr);
    sprintf(display_str, "GyrY: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 9, display_str);

    float_to_str(data.gyro_z, fstr);
    sprintf(display_str, "GyrZ: %s", fstr);
    PD_DrawString(text_x, start_y + line_height * 10, display_str);

    // 温度数据
    PD_SetColor(LCD_COLOR_WHITE);
    float_to_str(data.temperature, fstr);
    sprintf(display_str, "Temp: %s C", fstr);
    PD_DrawString(text_x, start_y + line_height * 12, display_str);

    // 底部提示
    PD_SetColor(LCD_COLOR_GRAY);
    PD_DrawString(text_x, 220, "Enter: Exit");

    // 刷新到屏幕
    LCD_Flush();
  }
}