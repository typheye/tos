/**
 ******************************************************************************
 * @file    lcd.cpp
 * @author  Typheye
 * @brief   Lcd implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/lcd.hpp"
#include "dram.h"
#include "library/include/libdly.h"

extern TIM_HandleTypeDef htim4;
LCD boardLCD;

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;

static uint16_t *lcd_tile_buffer = nullptr;
static volatile uint8_t lcd_debug_overlay_suppressed = 0;
static volatile uint8_t lcd_preserve_sah_splash = 1;

static void lcd_backlight_pwm_ensure(void) {
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_TIM4_CLK_ENABLE();

  const uint32_t mode = (GPIOD->MODER >> (13U * 2U)) & 0x3U;
  const uint32_t af = (GPIOD->AFR[1] >> ((13U - 8U) * 4U)) & 0xFU;

  /* Recover if the early splash helper left PD13 as a plain GPIO. */
  if (mode != 0x2U || af != GPIO_AF2_TIM4) {
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOD, &gpio);
  }

  if ((TIM4->CCER & TIM_CCER_CC2E) == 0U) {
    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  }
  TIM4->CR1 |= TIM_CR1_CEN;
}

extern "C" void SysUI_DebugOverlayBeginFrame(void);
extern "C" void SysUI_DebugOverlayEndFrame(void);
extern "C" void SysUI_DebugOverlayDraw(void);

__attribute__((weak)) void lcd_dma_yield(void) {}

#define LCD_HSPI hspi1

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

void LCD_BeginTileRender(uint16_t y, uint16_t h) {
  boardLCD.beginTileRender(y, h);
}
void LCD_EndTileRender(void) { boardLCD.endTileRender(); }
void LCD_FlushTiled(void (*render_cb)(void)) { boardLCD.flushTiled(render_cb); }
void LCD_SetDebugOverlaySuppressed(uint8_t suppressed) {
  lcd_debug_overlay_suppressed = suppressed ? 1U : 0U;
}
void LCD_EmergencyPrepare(void) { boardLCD.emergencyPrepare(); }
void LCD_FlushTiledBlocking(void (*render_cb)(void)) {
  boardLCD.flushTiledBlocking(render_cb);
}
void LCD_FlushFull(const uint16_t *data) { boardLCD.flushFull(data); }

void LCD_ClearFrameBuffer(uint32_t color) {
  uint16_t color_565 = boardLCD.rgb888ToRgb565(color);
  uint16_t *fb = boardLCD.getFrameBuffer();
  if (fb) {
    for (uint32_t i = 0; i < LCD_WIDTH * TILE_HEIGHT; i++) {
      fb[i] = color_565;
    }
  }
}

uint16_t LCD_RGB888ToRGB565(uint32_t rgb888) {
  return boardLCD.rgb888ToRgb565(rgb888);
}

uint16_t LCD_GetWidth(void) { return LCD_WIDTH; }
uint16_t LCD_GetHeight(void) { return LCD_HEIGHT; }

uint16_t LCD_GetTileY(void) { return boardLCD.getTileY(); }
uint16_t LCD_GetTileH(void) { return boardLCD.getTileH(); }

#ifdef __cplusplus
}
#endif

LCD::LCD() {
  _initialized = false;
  _currentColor565 = 0xFFFF;
  _rotation = 0;
  _brightnessPwm = 1000;
  _auto_brightness = false;
  _autoBrightnessLogged = false;
  _autoBrightnessForce = false;
  _autoBrightnessLuxValid = false;
  _autoBrightnessLast = 0;
  _autoBrightnessLux = 0.0f;
  _tileY = 0;
  _tileH = TILE_HEIGHT;
}

uint16_t LCD::rgb888ToRgb565(uint32_t rgb888) {
  uint8_t r = (rgb888 >> 16) & 0xFF;
  uint8_t g = (rgb888 >> 8) & 0xFF;
  uint8_t b = rgb888 & 0xFF;
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void LCD::hardwareReset(void) {
#ifdef LCD_RST_PIN
  LCD_RST_H;
  JPDelay(10);
  LCD_RST_L;
  JPDelay(10);
  LCD_RST_H;
  JPDelay(120);
#endif
}

void LCD::writeCmd(uint8_t cmd) {
  LCD_DC_CMD;
  LCD_CS_L;
  HAL_SPI_Transmit(&LCD_HSPI, &cmd, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}

void LCD::writeData(uint8_t data) {
  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&LCD_HSPI, &data, 1, HAL_MAX_DELAY);
  LCD_CS_H;
}

void LCD::writeData16(uint16_t data) {
  uint8_t buf[2];
  buf[0] = data >> 8;
  buf[1] = data & 0xFF;

  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&LCD_HSPI, buf, 2, HAL_MAX_DELAY);
  LCD_CS_H;
}

void LCD::setAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
  writeCmd(0x2A);
  writeData16(x1);
  writeData16(x2);

  writeCmd(0x2B);
  writeData16(y1);
  writeData16(y2);

  writeCmd(0x2C);
}

void LCD::setColor(uint32_t color) {
  _currentColor565 = rgb888ToRgb565(color);
}

void LCD::drawPixel(uint16_t x, uint16_t y, uint32_t color) {
  if (!_initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT)
    return;

  setAddress(x, y, x, y);
  writeData16(rgb888ToRgb565(color));
}

void LCD::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   uint32_t color) {
  if (!_initialized || x >= LCD_WIDTH || y >= LCD_HEIGHT || w == 0 || h == 0)
    return;

  if (x + w > LCD_WIDTH)
    w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT)
    h = LCD_HEIGHT - y;
  if (w == 0 || h == 0)
    return;

  uint16_t c565 = rgb888ToRgb565(color);
  setAddress(x, y, x + w - 1, y + h - 1);

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
    HAL_SPI_Transmit(&LCD_HSPI, buf, (uint16_t)(batch * 2), HAL_MAX_DELAY);
    remaining -= batch;
  }
  LCD_CS_H;
}

void LCD::init() {
  if (!lcd_tile_buffer) {
    lcd_tile_buffer = (uint16_t *)SysDram_AllocDma(sizeof(uint16_t) *
                                                   LCD_WIDTH * TILE_HEIGHT);
    if (!lcd_tile_buffer) {
      _initialized = false;
      return;
    }
  }

  uint8_t preserve_sah_splash = lcd_preserve_sah_splash;
  lcd_preserve_sah_splash = 0U;

  if (!preserve_sah_splash) {
    hardwareReset();

    writeCmd(0x01);
    JPDelay(150);

    writeCmd(0x11);
    JPDelay(120);
  }

  writeCmd(0x3A);
  writeData(0x55);

  writeCmd(0x36);
  writeData(0x10);

  writeCmd(0xB2);
  writeData(0x0C);
  writeData(0x0C);
  writeData(0x00);
  writeData(0x33);
  writeData(0x33);

  writeCmd(0xB7);
  writeData(0x35);

  writeCmd(0xBB);
  writeData(0x19);

  writeCmd(0xC0);
  writeData(0x2C);

  writeCmd(0xC2);
  writeData(0x01);

  writeCmd(0xC3);
  writeData(0x12);

  writeCmd(0xC4);
  writeData(0x20);

  writeCmd(0xC6);
  writeData(0x0F);

  writeCmd(0xD0);
  writeData(0xA4);
  writeData(0xA1);

  writeCmd(0xE0);
  writeData(0xD0);
  writeData(0x04);
  writeData(0x0D);
  writeData(0x11);
  writeData(0x13);
  writeData(0x2B);
  writeData(0x3F);
  writeData(0x54);
  writeData(0x4C);
  writeData(0x18);
  writeData(0x0D);
  writeData(0x0B);
  writeData(0x1F);
  writeData(0x23);

  writeCmd(0xE1);
  writeData(0xD0);
  writeData(0x04);
  writeData(0x0C);
  writeData(0x11);
  writeData(0x13);
  writeData(0x2C);
  writeData(0x3F);
  writeData(0x44);
  writeData(0x51);
  writeData(0x2F);
  writeData(0x1F);
  writeData(0x1F);
  writeData(0x20);
  writeData(0x23);

  writeCmd(0x21);
  writeCmd(0x29);
  JPDelay(100);

  if (!preserve_sah_splash) {
    LCD_BL_OFF;

    uint16_t black = rgb888ToRgb565(LCD_COLOR_BLACK);
    setAddress(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    LCD_DC_DATA;
    LCD_CS_L;

    for (uint32_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
      uint8_t buf[2];
      buf[0] = black >> 8;
      buf[1] = black & 0xFF;
      HAL_SPI_Transmit(&LCD_HSPI, buf, 2, HAL_MAX_DELAY);
    }

    LCD_CS_H;
    JPDelay(100);
  }

  LCD_BL_ON;
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  setBrightness(1000); // default full brightness (SM overrides after boot)

  _initialized = true;
}

void LCD::fillScreen(uint32_t color) {
  if (!_initialized)
    return;
  fillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD::drawPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const uint16_t *colors) {
  if (!_initialized)
    return;

  setAddress(x, y, x + w - 1, y + h - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
    uint8_t buf[2];
    buf[0] = colors[i] >> 8;
    buf[1] = colors[i] & 0xFF;
    HAL_SPI_Transmit(&LCD_HSPI, buf, 2, HAL_MAX_DELAY);
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

  writeCmd(0x36);

  switch (_rotation) {
  case 0:
    writeData(0x10);
    break;
  case 1:
    writeData(0x60);
    break;
  case 2:
    writeData(0xC0);
    break;
  case 3:
    writeData(0xA0);
    break;
  }
}

void LCD::sleep(void) {
  writeCmd(0x10);
  JPDelay(120);
  LCD_BL_OFF;
}

void LCD::wakeup(void) {
  LCD_BL_ON;
  writeCmd(0x11);
  JPDelay(120);
}

void LCD::setBrightness(uint16_t val) {
  if (val > 1000)
    val = 1000;
  _brightnessPwm = val;
  lcd_backlight_pwm_ensure();

  /* Keep the public scale independent of the timer period. */
  const uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim4) + 1U;
  const uint32_t compare =
      (val >= 1000U) ? period : ((uint32_t)val * period) / 1000U;
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, compare);
}

void LCD::setAutoBrightness(bool on) {
  bool enabled = on ? true : false;
  if (_auto_brightness != enabled) {
    _autoBrightnessForce = enabled;
    _autoBrightnessLast = 0;
    _autoBrightnessLuxValid = false;
  } else if (enabled) {
    _autoBrightnessForce = true;
  }
  _auto_brightness = enabled;
  if (!enabled) {
    _autoBrightnessLogged = false;
    _autoBrightnessLuxValid = false;
  }
}

static uint16_t lcd_auto_brightness_target(float lux) {
  if (lux < 3.0f)
    return 85U;
  if (lux < 10.0f)
    return 135U;
  if (lux < 30.0f)
    return 210U;
  if (lux < 80.0f)
    return 320U;
  if (lux < 180.0f)
    return 460U;
  if (lux < 400.0f)
    return 620U;
  if (lux < 900.0f)
    return 780U;
  if (lux < 2500.0f)
    return 900U;
  return 1000U;
}

void LCD::updateAutoBrightness(void) {
  uint32_t now = HAL_GetTick();

  if (!_auto_brightness) {
    _autoBrightnessLogged = false;
    _autoBrightnessLast = 0;
    _autoBrightnessLuxValid = false;
    return;
  }
  if (!_autoBrightnessLogged) {
    LOG_I("LCD", "Auto-brightness active");
    _autoBrightnessLogged = true;
  }

  const uint32_t interval = _autoBrightnessForce ? 0U : 300U;
  if ((uint32_t)(now - _autoBrightnessLast) < interval)
    return;
  _autoBrightnessLast = now;

  extern TCS3472 boardTCS3472;
  if (!boardTCS3472.isInitialized())
    return;

  float lux = boardTCS3472.getLux();
  if (lux < 0.0f || lux > 20000.0f) {
    return;
  }

  if (!_autoBrightnessLuxValid) {
    _autoBrightnessLux = lux;
    _autoBrightnessLuxValid = true;
  } else {
    float alpha = lux > _autoBrightnessLux ? 0.55f : 0.35f;
    if (lux < _autoBrightnessLux && lux < 30.0f)
      alpha = 0.25f;
    _autoBrightnessLux += (lux - _autoBrightnessLux) * alpha;
  }

  uint16_t target = lcd_auto_brightness_target(_autoBrightnessLux);

  uint16_t cur = _brightnessPwm;
  uint16_t distance = target > cur ? (uint16_t)(target - cur)
                                   : (uint16_t)(cur - target);

  if (distance < 25U) {
    _autoBrightnessForce = false;
    return;
  }

  uint16_t pwm = cur;
  if (target > cur) {
    uint16_t d = (uint16_t)(target - cur);
    uint16_t max_step = _autoBrightnessForce ? 1000U : 160U;
    pwm = (uint16_t)(cur + (d > max_step ? max_step : d));
  } else {
    uint16_t d = (uint16_t)(cur - target);
    uint16_t max_step = _autoBrightnessLux < 30.0f ? 70U : 120U;
    pwm = (uint16_t)(cur - (d > max_step ? max_step : d));
  }

  _autoBrightnessForce = false;

  if (pwm != _brightnessPwm)
    setBrightness(pwm);
}

uint16_t *LCD::getFrameBuffer(void) { return lcd_tile_buffer; }

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
  _tileY = y;
  _tileH = h;
  PD_SetTileWindow(y, h);
  EMO_SetTileWindow(y, h);

  uint16_t bg = rgb888ToRgb565(LCD_COLOR_BLACK);
  for (uint32_t i = 0; i < LCD_WIDTH * h; i++) {
    lcd_tile_buffer[i] = bg;
  }
}

void LCD::endTileRender(void) {
  if (!_initialized || !lcd_tile_buffer)
    return;

  const uint8_t last_tile = ((_tileY + _tileH) >= LCD_HEIGHT) ? 1U : 0U;
  if (!lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayDraw();
  }
  setAddress(0, _tileY, LCD_WIDTH - 1, _tileY + _tileH - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_DFF);
  LCD_HSPI.Init.DataSize = SPI_DATASIZE_16BIT;
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);

  MODIFY_REG(hdma_spi1_tx.Instance->CR, DMA_SxCR_MSIZE | DMA_SxCR_PSIZE,
             DMA_SxCR_MSIZE_0 | DMA_SxCR_PSIZE_0);
  hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;

  uint16_t pixel_count = LCD_WIDTH * _tileH;
  HAL_SPI_Transmit_DMA(&LCD_HSPI, (uint8_t *)lcd_tile_buffer, pixel_count);

  uint32_t timeout = HAL_GetTick() + 100;
  while (HAL_SPI_GetState(&LCD_HSPI) != HAL_SPI_STATE_READY) {
    if (HAL_GetTick() > timeout)
      break;
    lcd_dma_yield();
  }

  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_DFF);
  LCD_HSPI.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);

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
   * Instead, resync the SPI/DMA state, wake the already-_initialized panel, and
   * force full backlight. */
  HAL_SPI_Abort(&LCD_HSPI);
  HAL_DMA_Abort(&hdma_spi1_tx);
  LCD_CS_H;
  LCD_DC_DATA;

  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_DFF);
  LCD_HSPI.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);

  LCD_BL_ON;
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  setBrightness(1000);

  if (_initialized) {
    writeCmd(0x11); /* sleep out */
    JPDelay(20);
    writeCmd(0x3A); /* RGB565 */
    writeData(0x55);
    writeCmd(0x36); /* same default orientation as LCD::init() */
    writeData(0x10);
    writeCmd(0x21); /* display inversion on: matches normal TOS init */
    writeCmd(0x29); /* display on */
    JPDelay(20);
  }
}

void LCD::endTileRenderBlocking(void) {
  if (!_initialized || !lcd_tile_buffer)
    return;

  const uint8_t last_tile = ((_tileY + _tileH) >= LCD_HEIGHT) ? 1U : 0U;
  if (!lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayDraw();
  }
  setAddress(0, _tileY, LCD_WIDTH - 1, _tileY + _tileH - 1);
  LCD_DC_DATA;
  LCD_CS_L;

  HAL_SPI_Abort(&LCD_HSPI);
  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_DFF);
  LCD_HSPI.Init.DataSize = SPI_DATASIZE_16BIT;
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);

  uint16_t pixel_count = LCD_WIDTH * _tileH;
  (void)HAL_SPI_Transmit(&LCD_HSPI, (uint8_t *)lcd_tile_buffer, pixel_count, 500);

  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  CLEAR_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_DFF);
  LCD_HSPI.Init.DataSize = SPI_DATASIZE_8BIT;
  SET_BIT(LCD_HSPI.Instance->CR1, SPI_CR1_SPE);
  LCD_CS_H;
  if (last_tile && !lcd_debug_overlay_suppressed) {
    SysUI_DebugOverlayEndFrame();
  }
}

void LCD::flushTiledBlocking(void (*render_cb)(void)) {
  if (!_initialized || !render_cb)
    return;

  for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
    beginTileRender(y, h);
    render_cb();
    endTileRenderBlocking();
  }
}

void LCD::flushTiled(void (*render_cb)(void)) {
  if (!_initialized || !render_cb)
    return;

  for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
    uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
    beginTileRender(y, h);
    render_cb();
    endTileRender();
  }
}

void LCD::flushFull(const uint16_t *data) {
  if (!_initialized || !data)
    return;

  setAddress(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  LCD_DC_DATA;
  LCD_CS_L;

  LCD_HSPI.Init.DataSize = SPI_DATASIZE_16BIT;
  HAL_SPI_Init(&LCD_HSPI);

  HAL_SPI_Transmit(&LCD_HSPI, (uint8_t *)data, LCD_WIDTH * LCD_HEIGHT,
                   HAL_MAX_DELAY);

  LCD_HSPI.Init.DataSize = SPI_DATASIZE_8BIT;
  HAL_SPI_Init(&LCD_HSPI);

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
  writeCmd(0x2A);
  writeData16(start);
  writeData16(end);
}

void LCD::setRow(uint16_t start, uint16_t end) {
  writeCmd(0x2B);
  writeData16(start);
  writeData16(end);
}

void LCD::writeData(const uint8_t *data, uint32_t len) {
  LCD_DC_DATA;
  LCD_CS_L;
  HAL_SPI_Transmit(&LCD_HSPI, (uint8_t *)data, len, HAL_MAX_DELAY);
  LCD_CS_H;
}
