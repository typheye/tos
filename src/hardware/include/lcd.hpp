/**
 ******************************************************************************
 * @file    lcd.hpp
 * @author  Typheye
 * @brief   Lcd interface.
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

#ifndef __LCD_HPP
#define __LCD_HPP

#include "lcd.h"
#include "main.h"
#include "spi.h"
#include "stm32f4xx_hal.h"

class LCD {
public:
  LCD();
  void init();
  void fillScreen(uint32_t color);
  void setColor(uint32_t color);
  void drawPixel(uint16_t x, uint16_t y, uint32_t color);
  void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);

  
  void drawPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  const uint16_t *colors);

  
  void scrollArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t dx,
                  int16_t dy);

  
  void setRotation(uint8_t rotation);

  
  void sleep(void);
  void wakeup(void);

  
  void setBrightness(uint16_t val);
  uint16_t getBrightness(void) const { return _brightness_pwm; }
  void setAutoBrightness(bool on) { _auto_brightness = on; }
  bool getAutoBrightness(void) const { return _auto_brightness; }
  void updateAutoBrightness(void);  // read TCS3472 and adjust PWM
  uint8_t getRotation(void) const { return _rotation; }

  
  uint16_t getWidth(void) const { return LCD_WIDTH; }
  uint16_t getHeight(void) const { return LCD_HEIGHT; }

  uint16_t rgb888_to_rgb565(uint32_t rgb888);

  
  uint16_t *getFrameBuffer(void);
  void beginTileRender(uint16_t y, uint16_t h);
  void endTileRender(void);
  void endTileRenderBlocking(void);
  void flushTiled(void (*render_cb)(void));
  void emergencyPrepare(void);
  void flushTiledBlocking(void (*render_cb)(void));

  
  template <typename F>
  void flushTiled(F &&render_cb) {
    for (uint16_t y = 0; y < LCD_HEIGHT; y += TILE_HEIGHT) {
      uint16_t h = (y + TILE_HEIGHT <= LCD_HEIGHT) ? TILE_HEIGHT : LCD_HEIGHT - y;
      beginTileRender(y, h);
      render_cb();
      endTileRender();
    }
  }

  
  void flushFull(const uint16_t *data);

  
  void drawDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               const uint16_t *data);

  
  void set_address(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  void writeData(const uint8_t *data, uint32_t len);

  uint16_t getTileY(void) const { return _tile_y; }
  uint16_t getTileH(void) const { return _tile_h; }

private:
  bool initialized;
  uint16_t current_color_565;
  uint16_t _rotation;
  uint16_t _brightness_pwm;
  bool _auto_brightness;
  uint16_t _tile_y;
  uint16_t _tile_h;
  uint16_t _tile_buffer[LCD_WIDTH * TILE_HEIGHT];

  void write_cmd(uint8_t cmd);
  void write_data(uint8_t data);
  void write_data_16(uint16_t data);
  void hardware_reset(void);
  void setColumn(uint16_t start, uint16_t end);
  void setRow(uint16_t start, uint16_t end);
};

extern LCD boardLCD;

#ifdef __cplusplus
extern "C" void SysWatchdog_Tick(void);
#endif



#define LCD_FLUSH(...) do { SysWatchdog_Tick(); boardLCD.flushTiled([&]() { __VA_ARGS__ }); SysWatchdog_Tick(); } while (0)

#endif
