#include "include/lcd.hpp"
#include "include/tcs3472.hpp"
#include "include/libpd.h"
#include "include/libemo.h"
#include "tim.h"
#include "syslog.h"
#include <stdio.h>

extern TIM_HandleTypeDef htim4;
LCD boardLCD;

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;

// 弱符号 — 应用层可覆盖此函数来在 DMA 等待期间轮询按键
__attribute__((weak)) void lcd_dma_yield(void) {}

#define hspi hspi1

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

void LCD_Flush(void) { boardLCD.endTileRender(); }

uint16_t *LCD_GetFrameBuffer(void) { return boardLCD.getFrameBuffer(); }

void LCD_BeginTileRender(uint16_t y, uint16_t h) { boardLCD.beginTileRender(y, h); }
void LCD_EndTileRender(void) { boardLCD.endTileRender(); }
void LCD_FlushTiled(void (*render_cb)(void)) { boardLCD.flushTiled(render_cb); }
void LCD_FlushFull(const uint16_t *data) { boardLCD.flushFull(data); }

void LCD_ClearFrameBuffer(uint32_t color) {
  uint16_t color_565 = boardLCD.rgb888_to_rgb565(color);
  uint16_t *fb = boardLCD.getFrameBuffer();
  if (fb) {
    for (uint32_t i = 0; i < LCD_WIDTH * TILE_HEIGHT; i++) {
      fb[i] = color_565;
    }
  }
}

uint16_t LCD_RGB888ToRGB565(uint32_t rgb888) {
  return boardLCD.rgb888_to_rgb565(rgb888);
}

uint16_t LCD_GetWidth(void) { return LCD_WIDTH; }
uint16_t LCD_GetHeight(void) { return LCD_HEIGHT; }

uint16_t LCD_GetTileY(void) { return boardLCD.getTileY(); }
uint16_t LCD_GetTileH(void) { return boardLCD.getTileH(); }

#ifdef __cplusplus
}
#endif

// 构造函数
LCD::LCD() {
  initialized = false;
  current_color_565 = 0xFFFF;
  _auto_brightness = false;
  _tile_y = 0;
  _tile_h = TILE_HEIGHT;
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
  if (!initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0 || h == 0)
    return;

  if (x + w > LCD_WIDTH)
    w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT)
    h = LCD_HEIGHT - y;
  if (w == 0 || h == 0)
    return;

  uint16_t c565 = rgb888_to_rgb565(color);
  set_address(x, y, x + w - 1, y + h - 1);

  const uint32_t pixels = (uint32_t)w * h;
  uint8_t buf[128];
  const uint32_t chunk_pixels = sizeof(buf) / 2;
  const uint8_t hi = c565 >> 8;
  const uint8_t lo = c565 & 0xFF;

  for (uint32_t i = 0; i < chunk_pixels; ++i) {
    buf[i * 2] = hi;
    buf[i * 2 + 1] = lo;
  }

  LCD_DC_DATA;
  LCD_CS_L;
  uint32_t remaining = pixels;
  while (remaining > 0) {
    uint32_t batch = remaining < chunk_pixels ? remaining : chunk_pixels;
    HAL_SPI_Transmit(&hspi, buf, (uint16_t)(batch * 2), HAL_MAX_DELAY);
    remaining -= batch;
  }
  LCD_CS_H;
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
  static uint32_t last = 0;
  uint32_t now = HAL_GetTick();

  if (!_auto_brightness) {
    logged = false;
    return;
  }
  if (!logged) {
    LOG_I("LCD", "Auto-brightness active");
    logged = true;
  }

  /* Called both from UI drawing and SysWatchdog_Tick().  Keep it low-rate so
   * network-heavy periods do not starve brightness, but brightness updates also
   * do not add visible jitter to HTTP. */
  if ((uint32_t)(now - last) < 2000U) return;
  last = now;

  extern TCS3472 boardTCS3472;
  if (!boardTCS3472.isInitialized()) return;

  float lux = boardTCS3472.getLux();
  uint16_t target;
  if (lux < 1)          target = 50;
  else if (lux < 10)    target = 100;
  else if (lux < 50)    target = 200;
  else if (lux < 200)   target = 350;
  else if (lux < 500)   target = 550;
  else if (lux < 1000)  target = 750;
  else                  target = 1000;

  /* Smooth changes: full jumps are harsh and can look like the backlight is
   * fighting the UI while network work is running. */
  uint16_t cur = _brightness_pwm;
  uint16_t pwm;
  if (target > cur) {
    uint16_t d = target - cur;
    pwm = cur + (d > 120U ? 120U : d);
  } else {
    uint16_t d = cur - target;
    pwm = cur - (d > 120U ? 120U : d);
  }

  if (pwm != _brightness_pwm) setBrightness(pwm);
}

// ==================== 分块帧缓冲实现 ====================

uint16_t *LCD::getFrameBuffer(void) {
  return _tile_buffer;
}

void LCD::beginTileRender(uint16_t y, uint16_t h) {
  if (y >= LCD_HEIGHT)
    y = LCD_HEIGHT - 1;
  if (h == 0)
    h = 1;
  if (y + h > LCD_HEIGHT)
    h = LCD_HEIGHT - y;
  _tile_y = y;
  _tile_h = h;
  PD_SetTileWindow(y, h);
  EMO_SetTileWindow(y, h);
  // 清空 tile buffer
  uint16_t bg = rgb888_to_rgb565(LCD_COLOR_BLACK);
  for (uint32_t i = 0; i < LCD_WIDTH * h; i++) {
    _tile_buffer[i] = bg;
  }
}

void LCD::endTileRender(void) {
  if (!initialized)
    return;

  set_address(0, _tile_y, LCD_WIDTH - 1, _tile_y + _tile_h - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  // 快速切 SPI 到 16 位模式（直接操作寄存器，跳过 HAL_Init）
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  SET_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  // 快速切 DMA 到半字对齐
  MODIFY_REG(hdma_spi1_tx.Instance->CR, DMA_SxCR_MSIZE | DMA_SxCR_PSIZE,
             DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0);
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;

  uint16_t pixel_count = LCD_WIDTH * _tile_h;
  HAL_SPI_Transmit_DMA(&hspi, (uint8_t *)_tile_buffer, pixel_count);

  // 等待 DMA 完成（期间轮询按键）
  uint32_t timeout = HAL_GetTick() + 100;
  while (HAL_SPI_GetState(&hspi) != HAL_SPI_STATE_READY) {
    if (HAL_GetTick() > timeout) break;
    lcd_dma_yield();
  }

  // 快速恢复 8 位 SPI
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  // 快速恢复 DMA 字节对齐
  CLEAR_BIT(hdma_spi1_tx.Instance->CR, DMA_SxCR_MSIZE | DMA_SxCR_PSIZE);
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;

  LCD_CS_H;
}

void LCD::flushTiled(void (*render_cb)(void)) {
  if (!initialized || !render_cb)
    return;

  for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
    beginTileRender(y, h);
    render_cb();
    endTileRender();
  }
}

void LCD::flushFull(const uint16_t *data) {
  if (!initialized || !data)
    return;

  set_address(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  HAL_SPI_Init(&hspi);

  HAL_SPI_Transmit(&hspi, (uint8_t *)data, LCD_WIDTH * LCD_HEIGHT, HAL_MAX_DELAY);

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
