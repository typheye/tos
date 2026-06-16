/**
 ******************************************************************************
 * @file    lcd.cpp
 * @author  Typheye
 * @brief   Lcd implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "include/lcd.hpp"
#include "core/sys/include/sysdram.h"
#include "library/include/libdly.h"


extern TIM_HandleTypeDef htim4;
LCD boardLCD;

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;

static uint16_t *lcd_tile_buffer = nullptr;
static volatile uint8_t lcd_debug_overlay_suppressed = 0;
static volatile uint8_t lcd_preserve_sah_splash = 1;

extern "C" void SysUI_DebugOverlayBeginFrame(void);
extern "C" void SysUI_DebugOverlayEndFrame(void);
extern "C" void SysUI_DebugOverlayDraw(void);

__attribute__((weak)) void lcd_dma_yield(void) {}

#define hspi hspi1


#ifdef __cplusplus
extern "C" {
#endif

void LCD_Init(void) { boardLCD.init(); }
void LCD_SetSahSplashPreserve(uint8_t preserve) {
  lcd_preserve_sah_splash = preserve ? 1U : 0U;
}
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
void LCD_SetDebugOverlaySuppressed(uint8_t suppressed) {
  lcd_debug_overlay_suppressed = suppressed ? 1U : 0U;
}
void LCD_EmergencyPrepare(void) { boardLCD.emergencyPrepare(); }
void LCD_FlushTiledBlocking(void (*render_cb)(void)) { boardLCD.flushTiledBlocking(render_cb); }
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


LCD::LCD() {
  initialized = false;
  current_color_565 = 0xFFFF;
  _rotation = 0;
  _brightness_pwm = 1000;
  _auto_brightness = false;
  _auto_brightness_logged = false;
  _auto_brightness_force = false;
  _auto_brightness_last = 0;
  _tile_y = 0;
  _tile_h = TILE_HEIGHT;
}


uint16_t LCD::rgb888_to_rgb565(uint32_t rgb888) {
  uint8_t r = (rgb888 >> 16) & 0xFF;
  uint8_t g = (rgb888 >> 8) & 0xFF;
  uint8_t b = rgb888 & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}


void LCD::hardware_reset(void) {
#ifdef LCD_RST_PIN
  LCD_RST_H;
  JPDelay(10);
  LCD_RST_L;
  JPDelay(10);
  LCD_RST_H;
  JPDelay(120);
#endif
}


void LCD::write_cmd(uint8_t cmd) {
  LCD_DC_CMD;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, &cmd, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}


void LCD::write_data(uint8_t data) {
  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, &data, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}


void LCD::write_data_16(uint16_t data) {
  uint8_t buf[2];
  buf[0] = data >> 8;
  buf[1] = data & 0xFF;

  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
  LCD_CS_H;
}


void LCD::set_address(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  write_cmd(0x2A);
  write_data_16(x1);
  write_data_16(x2);

  write_cmd(0x2B);
  write_data_16(y1);
  write_data_16(y2);

  write_cmd(0x2C);
}


void LCD::setColor(uint32_t color) {
  current_color_565 = rgb888_to_rgb565(color);
}


void LCD::drawPixel(uint16_t x, uint16_t y, uint32_t color) {
  if (!initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;

  set_address(x, y, x, y);
  write_data_16(rgb888_to_rgb565(color));
}


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


void LCD::init() {
  if (!lcd_tile_buffer) {
    lcd_tile_buffer =
        (uint16_t *)SysDram_AllocDma(sizeof(uint16_t) * LCD_WIDTH * TILE_HEIGHT);
    if (!lcd_tile_buffer) {
      initialized = false;
      return;
    }
  }

  uint8_t preserve_sah_splash = lcd_preserve_sah_splash;
  lcd_preserve_sah_splash = 0U;

  if (!preserve_sah_splash) {
    hardware_reset();

    write_cmd(0x01);
    JPDelay(150);

    write_cmd(0x11);
    JPDelay(120);
  }

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
  JPDelay(100);

  
  
  if (!preserve_sah_splash) {
    LCD_BL_OFF;

    uint16_t black = rgb888_to_rgb565(LCD_COLOR_BLACK);
    set_address(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    LCD_DC_DATA;
    LCD_CS_L;

    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
      uint8_t buf[2];
      buf[0] = black >> 8;
      buf[1] = black & 0xFF;
      HAL_SPI_Transmit(&hspi, buf, 2, HAL_MAX_DELAY);
    }

    LCD_CS_H;
    JPDelay(100);
  }

  
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


void LCD::scrollArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t dx,
                     int16_t dy) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)dx;
  (void)dy;
}


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


void LCD::sleep(void) {
  write_cmd(0x10);
  JPDelay(120);
  LCD_BL_OFF;
}


void LCD::wakeup(void) {
  LCD_BL_ON;
  write_cmd(0x11);
  JPDelay(120);
}


void LCD::setBrightness(uint16_t val) {
  if (val > 1000)
    val = 1000;
  _brightness_pwm = val;
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, val);
}

void LCD::setAutoBrightness(bool on) {
  bool enabled = on ? true : false;
  if (_auto_brightness != enabled) {
    _auto_brightness_force = enabled;
    _auto_brightness_last = 0;
  } else if (enabled) {
    _auto_brightness_force = true;
  }
  _auto_brightness = enabled;
  if (!enabled) {
    _auto_brightness_logged = false;
  }
}

void LCD::updateAutoBrightness(void) {
  uint32_t now = HAL_GetTick();

  if (!_auto_brightness) {
    _auto_brightness_logged = false;
    _auto_brightness_last = 0;
    return;
  }
  if (!_auto_brightness_logged) {
    LOG_I("LCD", "Auto-brightness active");
    _auto_brightness_logged = true;
  }

  const uint32_t interval = _auto_brightness_force ? 0U : 300U;
  if ((uint32_t)(now - _auto_brightness_last) < interval) return;
  _auto_brightness_last = now;

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

  uint16_t cur = _brightness_pwm;
  uint16_t pwm;
  if (_auto_brightness_force) {
    pwm = target;
    _auto_brightness_force = false;
  } else if (target > cur) {
    uint16_t d = target - cur;
    pwm = cur + (d > 220U ? 220U : d);
  } else {
    uint16_t d = cur - target;
    pwm = cur - (d > 220U ? 220U : d);
  }

  if (pwm != _brightness_pwm) setBrightness(pwm);
}



uint16_t *LCD::getFrameBuffer(void) {
  return lcd_tile_buffer;
}

void LCD::beginTileRender(uint16_t y, uint16_t h) {
  if (!lcd_tile_buffer) {
    return;
  }
  if (y == 0U && !lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayBeginFrame();
  }
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
  
  uint16_t bg = rgb888_to_rgb565(LCD_COLOR_BLACK);
  for (uint32_t i = 0; i < LCD_WIDTH * h; i++) {
    lcd_tile_buffer[i] = bg;
  }
}

void LCD::endTileRender(void) {
  if (!initialized || !lcd_tile_buffer)
    return;

  const uint8_t last_tile = ((_tile_y + _tile_h) >= LCD_HEIGHT) ? 1U : 0U;
  if (!lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayDraw();
  }
  set_address(0, _tile_y, LCD_WIDTH - 1, _tile_y + _tile_h - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  SET_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  
  MODIFY_REG(hdma_spi1_tx.Instance->CR, DMA_SxCR_MSIZE | DMA_SxCR_PSIZE,
             DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0);
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;

  uint16_t pixel_count = LCD_WIDTH * _tile_h;
  HAL_SPI_Transmit_DMA(&hspi, (uint8_t *)lcd_tile_buffer, pixel_count);

  
  uint32_t timeout = HAL_GetTick() + 100;
  while (HAL_SPI_GetState(&hspi) != HAL_SPI_STATE_READY) {
    if (HAL_GetTick() > timeout) break;
    lcd_dma_yield();
  }

  
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  
  CLEAR_BIT(hdma_spi1_tx.Instance->CR, DMA_SxCR_MSIZE | DMA_SxCR_PSIZE);
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;

  LCD_CS_H;
  if (last_tile && !lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayEndFrame();
  }
}


void LCD::emergencyPrepare(void) {
  /* Fatal errors may be raised while SPI/DMA is still busy.  Do not run the
   * full LCD::init() here: it clears the panel pixel-by-pixel and can take long
   * enough for a short IWDG configuration to reset before anything is visible.
   * Instead, resync the SPI/DMA state, wake the already-initialized panel, and
   * force full backlight. */
  HAL_SPI_Abort(&hspi);
  HAL_DMA_Abort(&hdma_spi1_tx);
  LCD_CS_H;
  LCD_DC_DATA;

  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  LCD_BL_ON;
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  setBrightness(1000);

  if (initialized) {
    write_cmd(0x11); /* sleep out */
    JPDelay(20);
    write_cmd(0x29); /* display on */
    JPDelay(20);
  }
}

void LCD::endTileRenderBlocking(void) {
  if (!initialized || !lcd_tile_buffer) return;

  const uint8_t last_tile = ((_tile_y + _tile_h) >= LCD_HEIGHT) ? 1U : 0U;
  if (!lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayDraw();
  }
  set_address(0, _tile_y, LCD_WIDTH - 1, _tile_y + _tile_h - 1);
  LCD_DC_DATA;
  LCD_CS_L;

  HAL_SPI_Abort(&hspi);
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  SET_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_16BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);

  uint16_t pixel_count = LCD_WIDTH * _tile_h;
  (void)HAL_SPI_Transmit(&hspi, (uint8_t *)lcd_tile_buffer, pixel_count, 500);

  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(hspi.Instance->CR1, SPI_CR1_DFF);
  hspi.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(hspi.Instance->CR1, SPI_CR1_SPE);
  LCD_CS_H;
  if (last_tile && !lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayEndFrame();
  }
}

void LCD::flushTiledBlocking(void (*render_cb)(void)) {
  if (!initialized || !render_cb) return;

  for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
    beginTileRender(y, h);
    render_cb();
    endTileRenderBlocking();
  }
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


void LCD::drawDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  const uint16_t *data) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)data;
}


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
