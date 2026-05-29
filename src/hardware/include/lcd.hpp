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

  // 批量绘制接口
  void drawPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  const uint16_t *colors);

  // 滚动区域
  void scrollArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int16_t dx,
                  int16_t dy);

  // 屏幕方向 (0-3)
  void setRotation(uint8_t rotation);

  // 睡眠/唤醒
  void sleep(void);
  void wakeup(void);

  // 背光控制 (0-1000 PWM)
  void setBrightness(uint16_t val);
  uint16_t getBrightness(void) const { return _brightness_pwm; }
  uint8_t getRotation(void) const { return _rotation; }

  // 获取屏幕尺寸
  uint16_t getWidth(void) const { return LCD_WIDTH; }
  uint16_t getHeight(void) const { return LCD_HEIGHT; }

  uint16_t rgb888_to_rgb565(uint32_t rgb888);

  // 直接内存访问接口
  uint16_t *getFrameBuffer(void);
  void flush(void);

  // DMA 传输
  void drawDMA(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               const uint16_t *data);

  // 公开方法供 C 接口使用
  void set_address(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
  void writeData(const uint8_t *data, uint32_t len);

private:
  bool initialized;
  uint16_t current_color_565;
  uint16_t _rotation;
  uint16_t _brightness_pwm;
  uint16_t *_framebuffer;
  bool _use_framebuffer;

  void write_cmd(uint8_t cmd);
  void write_data(uint8_t data);
  void write_data_16(uint16_t data);
  void hardware_reset(void);
  void setColumn(uint16_t start, uint16_t end);
  void setRow(uint16_t start, uint16_t end);
};

extern LCD boardLCD;

#endif