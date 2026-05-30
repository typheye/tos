#include "include/lcd.hpp"
#include "include/tcs3472.hpp"
#include "tim.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim4;
LCD boardLCD;

extern SPI_HandleTypeDef hspi1;

#define hspi hspi1

static uint16_t *g_framebuffer = NULL;

// ==================== C 接口实现 ====================
#ifdef __cplusplus
extern "C" {
#endif

void LCD_Init(void) { boardLCD.init(); }
void LCD_UpdateAutoBrightness(void) { boardLCD.updateAutoBrightness(); }

void LCD_FillScreen(uint32_t color) { boardLCD.fillScreen(color); }

void LCD_DrawPixel(int16_t x, int16_t y, uint32_t color) {
  boardLCD.drawPixel(x, y, color);
}

void LCD_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
  boardLCD.fillRect(x, y, w, h, color);
}

void LCD_Flush(void) { boardLCD.flush(); }

uint16_t *LCD_GetFrameBuffer(void) { return boardLCD.getFrameBuffer(); }

void LCD_SetFrameBuffer(uint16_t *fb) { g_framebuffer = fb; }

void LCD_ClearFrameBuffer(uint32_t color) {
  uint16_t color_565 = boardLCD.rgb888_to_rgb565(color);
  uint16_t *fb = boardLCD.getFrameBuffer();
  if (fb) {
    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
      fb[i] = color_565;
    }
  }
}

uint16_t LCD_RGB888ToRGB565(uint32_t rgb888) {
  return boardLCD.rgb888_to_rgb565(rgb888);
}

uint16_t LCD_GetWidth(void) { return LCD_WIDTH; }
uint16_t LCD_GetHeight(void) { return LCD_HEIGHT; }

#ifdef __cplusplus
}
#endif

// 构造函数
LCD::LCD() {
  initialized = false;
  current_color_565 = 0xFFFF;
  _auto_brightness = false;
}

// RGB888 转 RGB565
uint16_t LCD::rgb888_to_rgb565(uint32_t rgb888) {
  uint8_t r = (rgb888 >> 16) & 0xFF;
  uint8_t g = (rgb888 >> 8) & 0xFF;
  uint8_t b = rgb888 & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// 硬件复位
void LCD::hardware_reset(void) {
#ifdef LCD_RST_PIN
  LCD_RST_H;
  HAL_Delay(10);
  LCD_RST_L;
  HAL_Delay(10);
  LCD_RST_H;
  HAL_Delay(120);
#endif
}

// 写命令
void LCD::write_cmd(uint8_t cmd) {
  LCD_DC_CMD;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, &cmd, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}

// 写8位数据
void LCD::write_data(uint8_t data) {
  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, &data, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}

// 写16位数据（大端序）
void LCD::write_data_16(uint16_t data) {
  uint8_t buf[2];
  buf[0] = data >> 8;
  buf[1] = data & 0xFF;

  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
  LCD_CS_H;
}

// 设置显示区域
void LCD::set_address(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  write_cmd(0x2A);
  write_data_16(x1);
  write_data_16(x2);

  write_cmd(0x2B);
  write_data_16(y1);
  write_data_16(y2);

  write_cmd(0x2C);
}

// 设置当前颜色
void LCD::setColor(uint32_t color) {
  current_color_565 = rgb888_to_rgb565(color);
}

// 画点
void LCD::drawPixel(uint16_t x, uint16_t y, uint32_t color) {
  if (!initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;

  set_address(x, y, x, y);
  write_data_16(rgb888_to_rgb565(color));
}

// 填充矩形
void LCD::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint32_t color) {
  if (!initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;

  if (x + w > LCD_WIDTH)
    w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT)
    h = LCD_HEIGHT - y;

  uint16_t c565 = rgb888_to_rgb565(color);
  set_address(x, y, x + w - 1, y + h - 1);

  const uint32_t pixels = (uint32_t)w * h;

  if (pixels <= 64) {
    LCD_DC_DATA;
    LCD_CS_L;
    uint8_t buf[2];
    buf[0] = c565 >> 8;
    buf[1] = c565 & 0xFF;
    for (uint32_t i = 0; i < pixels; i++) {
      HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
    }
    LCD_CS_H;
  } else {
    uint16_t *line_buffer = new uint16_t[w];
    for (uint16_t i = 0; i < w; i++) {
      line_buffer[i] = c565;
    }

    LCD_DC_DATA;
    LCD_CS_L;
    for (uint16_t row = 0; row < h; row++) {
      HAL_SPI_Transmit(&hspi, (uint8_t *)line_buffer, w * 2, HAL_MAX_DELAY);
    }
    LCD_CS_H;

    delete[] line_buffer;
  }
}

// LCD 初始化
void LCD::init() {
  hardware_reset();

  write_cmd(0x01);
  HAL_Delay(150);

  write_cmd(0x11);
  HAL_Delay(120);

  write_cmd(0x3A);
  write_data(0x55);

  write_cmd(0x36);
  write_data(0x10);

  write_cmd(0xB2);
  write_data(0x0C);
  write_data(0x0C);
  write_data(0x00);
  write_data(0x33);
  write_data(0x33);

  write_cmd(0xB7);
  write_data(0x35);

  write_cmd(0xBB);
  write_data(0x19);

  write_cmd(0xC0);
  write_data(0x2C);

  write_cmd(0xC2);
  write_data(0x01);

  write_cmd(0xC3);
  write_data(0x12);

  write_cmd(0xC4);
  write_data(0x20);

  write_cmd(0xC6);
  write_data(0x0F);

  write_cmd(0xD0);
  write_data(0xA4);
  write_data(0xA1);

  write_cmd(0xE0);
  write_data(0xD0);
  write_data(0x04);
  write_data(0x0D);
  write_data(0x11);
  write_data(0x13);
  write_data(0x2B);
  write_data(0x3F);
  write_data(0x54);
  write_data(0x4C);
  write_data(0x18);
  write_data(0x0D);
  write_data(0x0B);
  write_data(0x1F);
  write_data(0x23);

  write_cmd(0xE1);
  write_data(0xD0);
  write_data(0x04);
  write_data(0x0C);
  write_data(0x11);
  write_data(0x13);
  write_data(0x2C);
  write_data(0x3F);
  write_data(0x44);
  write_data(0x51);
  write_data(0x2F);
  write_data(0x1F);
  write_data(0x1F);
  write_data(0x20);
  write_data(0x23);

  write_cmd(0x21);
  write_cmd(0x29);
  HAL_Delay(100);

  // ========== 修复：直接调用底层函数，绕过 initialized 检查 ==========
  // 1. 关背光（确保看不到初始化过程）
  LCD_BL_OFF;

  // 2. 直接写黑色数据到屏幕（不经过 fillRect，因为 initialized = false）
  uint16_t black = rgb888_to_rgb565(LCD_COLOR_BLACK);
  set_address(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  // 填充整个屏幕为黑色
  for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
    uint8_t buf[2];
    buf[0] = black >> 8;
    buf[1] = black & 0xFF;
    HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
  }

  LCD_CS_H;

  // 3. 延迟等待数据写入完成
  HAL_Delay(100);

  // 4. 最后开背光
  LCD_BL_ON;
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  setBrightness(1000); // default full brightness (SM overrides after boot)

  initialized = true;
}

void LCD::fillScreen(uint32_t color) {
  if (!initialized)
    return;
  fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}
// ==================== 预留接口实现 ====================

// 批量绘制 (提高性能)
void LCD::drawPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint16_t *colors) {
  if (!initialized)
    return;

  set_address(x, y, x + w - 1, y + h - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
    uint8_t buf[2];
    buf[0] = colors[i] >> 8;
    buf[1] = colors[i] & 0xFF;
    HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
  }

  LCD_CS_H;
}

// 滚动区域
void LCD::scrollArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t dx,
                     int16_t dy) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)dx;
  (void)dy;
}

// 设置屏幕方向
void LCD::setRotation(uint8_t rotation) {
  _rotation = rotation % 4;

  write_cmd(0x36);

  switch (_rotation) {
  case 0:
    write_data(0x10);
    break;
  case 1:
    write_data(0x60);
    break;
  case 2:
    write_data(0xC0);
    break;
  case 3:
    write_data(0xA0);
    break;
  }
}

// 睡眠模式
void LCD::sleep(void) {
  write_cmd(0x10);
  HAL_Delay(120);
  LCD_BL_OFF;
}

// 唤醒
void LCD::wakeup(void) {
  LCD_BL_ON;
  write_cmd(0x11);
  HAL_Delay(120);
}

// 背光控制 (PWM TIM4 CH2, 0-1000)
void LCD::setBrightness(uint16_t val) {
  if (val > 1000)
    val = 1000;
  _brightness_pwm = val;
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, val);
}

// 自动亮度: 读取 TCS3472 环境光, 映射到 PWM
void LCD::updateAutoBrightness(void) {
  static bool logged = false;
  if (!_auto_brightness) { logged = false; return; }
  if (!logged) { printf("[LCD] Auto-brightness active\r\n"); logged = true; }
  static uint32_t last = 0;
  if (HAL_GetTick() - last < 2000) return;
  last = HAL_GetTick();

  extern TCS3472 boardTCS3472;
  if (!boardTCS3472.isInitialized()) return;

  float lux = boardTCS3472.getLux();
  uint16_t pwm;
  if (lux < 1)       pwm = 50;
  else if (lux < 10)  pwm = 100;
  else if (lux < 50)  pwm = 200;
  else if (lux < 200) pwm = 350;
  else if (lux < 500) pwm = 550;
  else if (lux < 1000) pwm = 750;
  else                pwm = 1000;

  if (pwm != _brightness_pwm) setBrightness(pwm);
}

// 获取帧缓冲区
uint16_t *LCD::getFrameBuffer(void) {
  // 临时方案：使用静态局部变量
  static uint16_t framebuffer[LCD_WIDTH * LCD_HEIGHT];
  if (_framebuffer == nullptr) {
    _framebuffer = framebuffer; // 指向静态数组，而不是 new
  }
  _use_framebuffer = true;
  return _framebuffer;
}

// 刷新帧缓冲区到屏幕
void LCD::flush(void) {
  if (_framebuffer == nullptr)
    return;

  set_address(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  // 配置 SPI 为 16 位模式
  hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  HAL_SPI_Init(&hspi);

  // 16 位传输，像素数量是 240*240 = 57600，在 uint16_t 范围内
  uint16_t pixel_count = LCD_WIDTH * LCD_HEIGHT;
  HAL_SPI_Transmit(&hspi, (uint8_t *)_framebuffer, pixel_count, HAL_MAX_DELAY);

  // 恢复 8 位模式
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  HAL_SPI_Init(&hspi);

  LCD_CS_H;
}

// DMA 传输
void LCD::drawDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  const uint16_t *data) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)data;
}

// 辅助函数
void LCD::setColumn(uint16_t start, uint16_t end) {
  write_cmd(0x2A);
  write_data_16(start);
  write_data_16(end);
}

void LCD::setRow(uint16_t start, uint16_t end) {
  write_cmd(0x2B);
  write_data_16(start);
  write_data_16(end);
}

void LCD::writeData(const uint8_t *data, uint32_t len) {
  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, (uint8_t *)data, len, HAL_MAX_DELAY);
  LCD_CS_H;
}