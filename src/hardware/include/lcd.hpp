/**
 ******************************************************************************
 * @file    lcd.hpp
 * @author  Typheye
 * @brief   Lcd interface.
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

#ifndef LCD_HPP
#define LCD_HPP

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
  uint16_t getBrightness(void) const { return _brightnessPwm; }
  void setAutoBrightness(bool on);
  bool getAutoBrightness(void) const { return _auto_brightness; }
  void updateAutoBrightness(void);  // read TCS3472 and adjust PWM
  uint8_t getRotation(void) const { return _rotation; }


  uint16_t getWidth(void) const { return LCD_WIDTH; }
  uint16_t getHeight(void) const { return LCD_HEIGHT; }

  uint16_t rgb888ToRgb565(uint32_t rgb888);


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


  void setAddress(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  void writeData(const uint8_t *data, uint32_t len);

  uint16_t getTileY(void) const { return _tileY; }
  uint16_t getTileH(void) const { return _tileH; }

private:
  bool _initialized;
  uint16_t _currentColor565;
  uint16_t _rotation;
  uint16_t _brightnessPwm;
  bool _auto_brightness;
  bool _autoBrightnessLogged;
  bool _autoBrightnessForce;
  bool _autoBrightnessLuxValid;
  uint32_t _autoBrightnessLast;
  float _autoBrightnessLux;
  uint16_t _tileY;
  uint16_t _tileH;

  void writeCmd(uint8_t cmd);
  void writeData(uint8_t data);
  void writeData16(uint16_t data);
  void hardwareReset(void);
  void setColumn(uint16_t start, uint16_t end);
  void setRow(uint16_t start, uint16_t end);
};

extern LCD boardLCD;

#ifdef __cplusplus
extern "C" void SysWatchdog_Tick(void);
#endif



#define LCD_FLUSH(...) do { SysWatchdog_Tick(); boardLCD.flushTiled([&]() { __VA_ARGS__; }); SysWatchdog_Tick(); } while (0)

#endif /* LCD_HPP */
