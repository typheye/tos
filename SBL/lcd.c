#include "sbl_lcd.h"

#include "sbl_hw.h"
#include "tim.h"

#define SBL_LCD_CS_PIN   11U
#define SBL_LCD_DC_PIN   12U
#define SBL_LCD_BL_PIN   13U
#define SBL_LCD_RST_PIN  14U

static SBL_CODE void sbl_lcd_cmd(uint8_t cmd) {
  SBL_GpioReset(GPIOD, SBL_LCD_DC_PIN);
  SBL_GpioReset(GPIOD, SBL_LCD_CS_PIN);
  SBL_Spi1Write(cmd);
  SBL_GpioSet(GPIOD, SBL_LCD_CS_PIN);
}

static SBL_CODE void sbl_lcd_data_bytes(const uint8_t *data, uint32_t len) {
  SBL_GpioSet(GPIOD, SBL_LCD_DC_PIN);
  SBL_GpioReset(GPIOD, SBL_LCD_CS_PIN);
  SBL_Spi1WriteBytes(data, len);
  SBL_GpioSet(GPIOD, SBL_LCD_CS_PIN);
}

static SBL_CODE void sbl_lcd_data(uint8_t data) {
  sbl_lcd_data_bytes(&data, 1U);
}

static SBL_CODE void sbl_lcd_data16(uint16_t data) {
  sbl_lcd_data((uint8_t)(data >> 8));
  sbl_lcd_data((uint8_t)data);
}

static SBL_CODE void sbl_lcd_addr(uint16_t x0, uint16_t y0,
                                  uint16_t x1, uint16_t y1) {
  sbl_lcd_cmd(0x2AU);
  sbl_lcd_data16(x0);
  sbl_lcd_data16(x1);
  sbl_lcd_cmd(0x2BU);
  sbl_lcd_data16(y0);
  sbl_lcd_data16(y1);
  sbl_lcd_cmd(0x2CU);
}

static SBL_CODE void sbl_lcd_push_color(uint16_t color, uint32_t pixels) {
  SBL_GpioSet(GPIOD, SBL_LCD_DC_PIN);
  SBL_GpioReset(GPIOD, SBL_LCD_CS_PIN);
  while (pixels--) {
    SBL_Spi1Write((uint8_t)(color >> 8));
    SBL_Spi1Write((uint8_t)color);
  }
  SBL_GpioSet(GPIOD, SBL_LCD_CS_PIN);
}

SBL_CODE void SBL_LcdRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint16_t color) {
  if (x >= SBL_LCD_W || y >= SBL_LCD_H || w == 0U || h == 0U) {
    return;
  }
  if ((uint32_t)x + w > SBL_LCD_W) {
    w = (uint16_t)(SBL_LCD_W - x);
  }
  if ((uint32_t)y + h > SBL_LCD_H) {
    h = (uint16_t)(SBL_LCD_H - y);
  }
  sbl_lcd_addr(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  sbl_lcd_push_color(color, (uint32_t)w * h);
}

static SBL_CODE void sbl_lcd_reset(void) {
  SBL_GpioSet(GPIOD, SBL_LCD_RST_PIN);
  SBL_DelayMs(10U);
  SBL_GpioReset(GPIOD, SBL_LCD_RST_PIN);
  SBL_DelayMs(10U);
  SBL_GpioSet(GPIOD, SBL_LCD_RST_PIN);
  SBL_DelayMs(120U);
}

SBL_CODE void SBL_LcdInit(void) {
  SBL_Spi1InitForLcd();
  sbl_lcd_reset();

  sbl_lcd_cmd(0x01U);
  SBL_DelayMs(150U);
  sbl_lcd_cmd(0x11U);
  SBL_DelayMs(120U);

  sbl_lcd_cmd(0x3AU);
  sbl_lcd_data(0x55U);
  sbl_lcd_cmd(0x36U);
  sbl_lcd_data(0x10U);

  static const uint8_t seq_b2[] SBL_CONST = {0x0C, 0x0C, 0x00, 0x33, 0x33};
  static const uint8_t seq_d0[] SBL_CONST = {0xA4, 0xA1};
  static const uint8_t seq_e0[] SBL_CONST = {
      0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
      0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
  static const uint8_t seq_e1[] SBL_CONST = {
      0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
      0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};

  sbl_lcd_cmd(0xB2U);
  sbl_lcd_data_bytes(seq_b2, sizeof(seq_b2));
  sbl_lcd_cmd(0xB7U);
  sbl_lcd_data(0x35U);
  sbl_lcd_cmd(0xBBU);
  sbl_lcd_data(0x19U);
  sbl_lcd_cmd(0xC0U);
  sbl_lcd_data(0x2CU);
  sbl_lcd_cmd(0xC2U);
  sbl_lcd_data(0x01U);
  sbl_lcd_cmd(0xC3U);
  sbl_lcd_data(0x12U);
  sbl_lcd_cmd(0xC4U);
  sbl_lcd_data(0x20U);
  sbl_lcd_cmd(0xC6U);
  sbl_lcd_data(0x0FU);
  sbl_lcd_cmd(0xD0U);
  sbl_lcd_data_bytes(seq_d0, sizeof(seq_d0));
  sbl_lcd_cmd(0xE0U);
  sbl_lcd_data_bytes(seq_e0, sizeof(seq_e0));
  sbl_lcd_cmd(0xE1U);
  sbl_lcd_data_bytes(seq_e1, sizeof(seq_e1));

  sbl_lcd_cmd(0x21U);
  sbl_lcd_cmd(0x29U);
  SBL_DelayMs(100U);
  (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 1000U);
}

static SBL_CODE uint8_t sbl_font5x7(char c, uint8_t col) {
  static const uint8_t blank[5] SBL_CONST = {0, 0, 0, 0, 0};
  static const uint8_t colon[5] SBL_CONST = {0x00, 0x36, 0x36, 0x00, 0x00};
  static const uint8_t dash[5] SBL_CONST = {0x08, 0x08, 0x08, 0x08, 0x08};
  static const uint8_t space[5] SBL_CONST = {0x00, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t underscore[5] SBL_CONST = {0x40, 0x40, 0x40, 0x40, 0x40};
  static const uint8_t slash[5] SBL_CONST = {0x40, 0x30, 0x08, 0x06, 0x01};
  static const uint8_t dot[5] SBL_CONST = {0x00, 0x60, 0x60, 0x00, 0x00};
  static const uint8_t comma[5] SBL_CONST = {0x00, 0x80, 0x60, 0x00, 0x00};
  static const uint8_t semicolon[5] SBL_CONST = {0x00, 0x80, 0x66, 0x00, 0x00};
  static const uint8_t exclaim[5] SBL_CONST = {0x00, 0x00, 0x5F, 0x00, 0x00};
  static const uint8_t question[5] SBL_CONST = {0x02, 0x01, 0x51, 0x09, 0x06};
  static const uint8_t lparen[5] SBL_CONST = {0x00, 0x1C, 0x22, 0x41, 0x00};
  static const uint8_t rparen[5] SBL_CONST = {0x00, 0x41, 0x22, 0x1C, 0x00};
  static const uint8_t plus[5] SBL_CONST = {0x08, 0x08, 0x3E, 0x08, 0x08};
  static const uint8_t equal[5] SBL_CONST = {0x14, 0x14, 0x14, 0x14, 0x14};
  static const uint8_t lt[5] SBL_CONST = {0x08, 0x14, 0x22, 0x41, 0x00};
  static const uint8_t gt[5] SBL_CONST = {0x00, 0x41, 0x22, 0x14, 0x08};
  static const uint8_t quote[5] SBL_CONST = {0x00, 0x07, 0x00, 0x07, 0x00};
  static const uint8_t apos[5] SBL_CONST = {0x00, 0x00, 0x07, 0x00, 0x00};
  static const uint8_t letters[26][5] SBL_CONST = {
      {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
      {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
      {0x7F,0x20,0x18,0x20,0x7F}, {0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}};
  static const uint8_t lowers[26][5] SBL_CONST = {
      {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38},
      {0x38,0x44,0x44,0x44,0x20}, {0x38,0x44,0x44,0x48,0x7F},
      {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02},
      {0x0C,0x52,0x52,0x52,0x3E}, {0x7F,0x08,0x04,0x04,0x78},
      {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00},
      {0x7F,0x10,0x28,0x44,0x00}, {0x00,0x41,0x7F,0x40,0x00},
      {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78},
      {0x38,0x44,0x44,0x44,0x38}, {0x7C,0x14,0x14,0x14,0x08},
      {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08},
      {0x48,0x54,0x54,0x54,0x20}, {0x04,0x3F,0x44,0x40,0x20},
      {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C},
      {0x3C,0x40,0x30,0x40,0x3C}, {0x44,0x28,0x10,0x28,0x44},
      {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}};
  static const uint8_t digits[10][5] SBL_CONST = {
      {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00},
      {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
      {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
      {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}};

  const uint8_t *glyph = blank;
  if (c >= 'A' && c <= 'Z') glyph = letters[c - 'A'];
  else if (c >= 'a' && c <= 'z') glyph = lowers[c - 'a'];
  else if (c >= '0' && c <= '9') glyph = digits[c - '0'];
  else if (c == ' ') glyph = space;
  else if (c == ':') glyph = colon;
  else if (c == '-') glyph = dash;
  else if (c == '_') glyph = underscore;
  else if (c == '/') glyph = slash;
  else if (c == '.') glyph = dot;
  else if (c == ',') glyph = comma;
  else if (c == ';') glyph = semicolon;
  else if (c == '!') glyph = exclaim;
  else if (c == '?') glyph = question;
  else if (c == '(') glyph = lparen;
  else if (c == ')') glyph = rparen;
  else if (c == '+') glyph = plus;
  else if (c == '=') glyph = equal;
  else if (c == '<') glyph = lt;
  else if (c == '>') glyph = gt;
  else if (c == '"') glyph = quote;
  else if (c == '\'') glyph = apos;
  return glyph[col];
}

static SBL_CODE void sbl_draw_char(uint16_t x, uint16_t y, char c,
                                   uint16_t color, uint8_t scale) {
  for (uint8_t col = 0; col < 5U; ++col) {
    uint8_t bits = sbl_font5x7(c, col);
    for (uint8_t row = 0; row < 7U; ++row) {
      if (bits & (1U << row)) {
        SBL_LcdRect((uint16_t)(x + col * scale),
                    (uint16_t)(y + row * scale), scale, scale, color);
      }
    }
  }
}

static SBL_CODE void sbl_draw_title_char(uint16_t x, uint16_t y, char c,
                                         uint16_t color) {
  static const uint8_t x_pos[5] SBL_CONST = {0U, 2U, 3U, 5U, 6U};
  static const uint8_t x_w[5] SBL_CONST = {2U, 1U, 2U, 1U, 2U};
  static const uint8_t y_pos[7] SBL_CONST = {0U, 2U, 3U, 5U, 6U, 8U, 9U};
  static const uint8_t y_h[7] SBL_CONST = {2U, 1U, 2U, 1U, 2U, 1U, 2U};

  for (uint8_t col = 0; col < 5U; ++col) {
    uint8_t bits = sbl_font5x7(c, col);
    for (uint8_t row = 0; row < 7U; ++row) {
      if (bits & (1U << row)) {
        SBL_LcdRect((uint16_t)(x + x_pos[col]), (uint16_t)(y + y_pos[row]),
                    x_w[col], y_h[row], color);
      }
    }
  }
}

SBL_CODE void SBL_LcdDrawText(uint16_t x, uint16_t y, const char *text,
                              uint16_t color, uint8_t scale) {
  uint16_t start_x = x;
  while (*text) {
    if (*text == '\n') {
      x = start_x;
      y = (uint16_t)(y + 12U * scale);
      text++;
      continue;
    }
    sbl_draw_char(x, y, *text++, color, scale);
    x = (uint16_t)(x + 6U * scale);
  }
}

SBL_CODE void SBL_LcdDrawTitleText(uint16_t x, uint16_t y, const char *text,
                                   uint16_t color) {
  while (*text) {
    sbl_draw_title_char(x, y, *text++, color);
    x = (uint16_t)(x + 9U);
  }
}
