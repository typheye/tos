#include "include/libemo.h"
#include "include/lcd.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

static CCMRAM uint16_t *g_fb = NULL;
static CCMRAM uint16_t g_w = 0;
static CCMRAM uint16_t g_h = 0;
static uint16_t g_tile_y = 0;
static uint16_t g_tile_h = 0;

static uint16_t rgb565(uint32_t c) {
  return (uint16_t)(((c >> 19) << 11) | (((c >> 10) & 0x3F) << 5) | ((c >> 3) & 0x1F));
}

static uint32_t mix_rgb(uint32_t a, uint32_t b, float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  uint8_t ar = (uint8_t)((a >> 16) & 0xFF);
  uint8_t ag = (uint8_t)((a >> 8) & 0xFF);
  uint8_t ab = (uint8_t)(a & 0xFF);
  uint8_t br = (uint8_t)((b >> 16) & 0xFF);
  uint8_t bg = (uint8_t)((b >> 8) & 0xFF);
  uint8_t bb = (uint8_t)(b & 0xFF);
  uint8_t r = (uint8_t)((float)ar + ((float)br - (float)ar) * t);
  uint8_t g = (uint8_t)((float)ag + ((float)bg - (float)ag) * t);
  uint8_t bl = (uint8_t)((float)ab + ((float)bb - (float)ab) * t);
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
}

// Tile-relative pixel access: returns pointer or NULL if outside current tile
static inline uint16_t *emo_px_ptr(int16_t x, int16_t y) {
  if (!g_fb || x < 0 || x >= (int)g_w || y < (int)g_tile_y || y >= (int)(g_tile_y + g_tile_h))
    return NULL;
  return &g_fb[(y - g_tile_y) * g_w + x];
}

#define FB(x,y) g_fb[((y) - g_tile_y) * g_w + (x)]

static void set_px(int16_t x, int16_t y, uint32_t color) {
  uint16_t *p = emo_px_ptr(x, y);
  if (p) *p = rgb565(color);
}

static inline int tile_intersects_y(int16_t y0, int16_t y1) {
  return y1 >= (int16_t)g_tile_y && y0 < (int16_t)(g_tile_y + g_tile_h);
}

void EMO_Init(void) {
  g_fb = LCD_GetFrameBuffer();
  g_w  = LCD_GetWidth();
  g_h  = TILE_HEIGHT;
  g_tile_y = LCD_GetTileY();
  g_tile_h = LCD_GetTileH();
}

void EMO_SetTileWindow(uint16_t y, uint16_t h) {
  g_tile_y = y;
  g_tile_h = h;
}

void EMO_FillScreen(uint32_t color) {
  if (!g_fb) return;
  uint16_t c = rgb565(color);
  uint32_t n = (uint32_t)g_w * g_tile_h;
  for (uint32_t i = 0; i < n; i++) g_fb[i] = c;
}

void EMO_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
  if (!g_fb) return;
  if (w <= 0 || h <= 0 || !tile_intersects_y(y, (int16_t)(y + h - 1))) return;
  // Clip Y to current tile
  if (y < (int)g_tile_y) { h -= (int)(g_tile_y - y); y = g_tile_y; }
  if (y + h > (int)(g_tile_y + g_tile_h)) h = (int)(g_tile_y + g_tile_h) - y;
  // Clip X
  if (x < 0) { w += x; x = 0; }
  if (x + w > g_w) w = g_w - x;
  if (w <= 0 || h <= 0) return;
  uint16_t c = rgb565(color);
  for (int16_t j = 0; j < h; j++)
    for (int16_t i = 0; i < w; i++)
      FB(x + i, y + j) = c;
}

void EMO_FillCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color) {
  if (!g_fb || r <= 0) return;
  if (!tile_intersects_y((int16_t)(cy - r), (int16_t)(cy + r))) return;
  uint16_t c = rgb565(color);
  int16_t r2 = r * r;
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t sy = cy + dy;
    if (sy < (int)g_tile_y || sy >= (int)(g_tile_y + g_tile_h)) continue;
    int16_t dx = (int16_t)sqrtf((float)(r2 - dy * dy));
    int16_t x0 = cx - dx; if (x0 < 0) x0 = 0;
    int16_t x1 = cx + dx; if (x1 >= g_w) x1 = g_w - 1;
    for (int16_t sx = x0; sx <= x1; sx++) FB(sx, sy) = c;
  }
}

void EMO_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color) {
  if (!g_fb || r <= 0) return;
  if (!tile_intersects_y((int16_t)(cy - r), (int16_t)(cy + r))) return;
  int16_t x = r, y = 0, err = 0;
  while (x >= y) {
    set_px(cx + x, cy + y, color); set_px(cx + y, cy + x, color);
    set_px(cx - y, cy + x, color); set_px(cx - x, cy + y, color);
    set_px(cx - x, cy - y, color); set_px(cx - y, cy - x, color);
    set_px(cx + y, cy - x, color); set_px(cx + x, cy - y, color);
    if (err <= 0) { y++; err += 2 * y + 1; }
    if (err > 0)  { x--; err -= 2 * x + 1; }
  }
}

void EMO_DrawHLine(int16_t x, int16_t y, int16_t len, int16_t t, uint32_t color) {
  if (!g_fb || len <= 0 || t <= 0) return;
  if (!tile_intersects_y((int16_t)(y - t / 2), (int16_t)(y + (t + 1) / 2))) return;
  uint16_t c = rgb565(color);
  int16_t y0 = y - t / 2;
  for (int16_t dy = 0; dy < t; dy++) {
    int16_t sy = y0 + dy;
    if (sy < (int)g_tile_y || sy >= (int)(g_tile_y + g_tile_h)) continue;
    for (int16_t dx = 0; dx < len; dx++) {
      int16_t sx = x + dx;
      if (sx >= 0 && sx < g_w) FB(sx, sy) = c;
    }
  }
}

void EMO_DrawThickArc(int16_t cx, int16_t cy, int16_t r, float s_deg, float e_deg, int16_t t, uint32_t color) {
  if (!g_fb || r <= 0 || t <= 0) return;
  if (!tile_intersects_y((int16_t)(cy - r - t), (int16_t)(cy + r + t))) return;
  float sr = s_deg * M_PI / 180.0f;
  float er = e_deg * M_PI / 180.0f;
  if (er < sr) er += 2.0f * M_PI;
  float span = er - sr;
  int steps = (int)(span * (float)r) + 1;
  if (steps < 16) steps = 16;
  for (int i = 0; i <= steps; i++) {
    float a = sr + span * (float)i / (float)steps;
    int16_t px = cx + (int16_t)(cosf(a) * r);
    int16_t py = cy - (int16_t)(sinf(a) * r);
    EMO_FillCircle(px, py, t, color);
  }
}

// ============ Small local helpers ============

static float clampf_emo(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void draw_sparkle(int16_t x, int16_t y, int16_t s, uint32_t color) {
  EMO_DrawHLine(x - s, y, s * 2 + 1, 1, color);
  for (int16_t i = -s; i <= s; i++) set_px(x, y + i, color);
}

static void draw_soft_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t t, uint32_t color) {
  int16_t min_y = y0 < y1 ? y0 : y1;
  int16_t max_y = y0 > y1 ? y0 : y1;
  if (!tile_intersects_y((int16_t)(min_y - t), (int16_t)(max_y + t))) return;
  int16_t dx = (int16_t)abs(x1 - x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = (int16_t)-abs(y1 - y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;
  while (1) {
    EMO_FillCircle(x0, y0, t, color);
    if (x0 == x1 && y0 == y1) break;
    int16_t e2 = (int16_t)(2 * err);
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void draw_z_mark(int16_t x, int16_t y, int16_t s, uint32_t color) {
  int16_t w = (int16_t)(s * 2);
  EMO_DrawHLine(x, y, w, 2, color);
  draw_soft_line((int16_t)(x + w - 1), (int16_t)(y + 1),
                 x, (int16_t)(y + s + 2), 1, color);
  EMO_DrawHLine(x, (int16_t)(y + s + 3), w, 2, color);
}

static void draw_sweat(int16_t x, int16_t y, int16_t s) {
  uint32_t c = 0x7FD9FF;
  EMO_FillCircle(x, (int16_t)(y + s), s, c);
  draw_soft_line(x, y, (int16_t)(x - s), (int16_t)(y + s + 1), 1, c);
  draw_soft_line(x, y, (int16_t)(x + s), (int16_t)(y + s + 1), 1, c);
  EMO_FillCircle((int16_t)(x - s / 2), (int16_t)(y + s / 2), 1, 0xE7FBFF);
}

// ============ Eye drawing (soft sclera + pupil + eyelids) ============

static void draw_eye(int16_t cx, int16_t cy, float blink, float lx, float ly) {
  int16_t r = EMO_EYE_R;
  blink = clampf_emo(blink, 0.0f, 1.0f);
  lx = clampf_emo(lx, -1.0f, 1.0f);
  ly = clampf_emo(ly, -1.0f, 1.0f);

  if (blink < 0.90f) {
    // Soft outside shadow makes the eye feel less flat.
    EMO_FillCircle(cx + 1, cy + 2, r + 1, 0x1A1A1A);
    EMO_FillCircle(cx, cy, r, EMO_WHITE);
    EMO_FillCircle(cx - r / 3, cy - r / 3, r / 3, 0xF4FBFF);

    if (blink < 0.76f) {
      int16_t max_off_x = r - EMO_PUPIL_R - 3;
      int16_t max_off_y = r - EMO_PUPIL_R - 4;
      int16_t px = cx + (int16_t)(lx * (float)max_off_x);
      int16_t py = cy + (int16_t)(ly * (float)max_off_y);
      EMO_FillCircle(px, py, EMO_PUPIL_R + 1, 0x101018);
      EMO_FillCircle(px, py, EMO_PUPIL_R, EMO_BLACK);
      EMO_FillCircle(px - 3, py - 3, 3, EMO_WHITE);
      EMO_FillCircle(px + 3, py + 2, 1, 0xC7D9FF);
    }

    // Upper eyelid and lower squeeze.  Drawing these last fixes pupil bleed.
    int16_t cover_top = (int16_t)(blink * r * 2.05f);
    if (cover_top > 0) {
      EMO_FillRect(cx - r - 2, cy - r - 1, r * 2 + 4, cover_top, EMO_BLACK);
    }
    int16_t cover_bottom = (int16_t)(blink * r * 0.40f);
    if (cover_bottom > 0) {
      EMO_FillRect(cx - r - 1, cy + r - cover_bottom, r * 2 + 2, cover_bottom + 2, EMO_BLACK);
    }
  } else {
    EMO_DrawThickArc(cx, (int16_t)(cy - 4), (int16_t)(r - 1), 205.0f, 335.0f, 2, EMO_WHITE);
    EMO_DrawHLine(cx - r + 7, cy + 3, r * 2 - 14, 1, 0x444444);
  }
}

// ============ Main face ============

void EMO_DrawFace(float blink_l, float blink_r, float mouth_open,
                  float look_x, float look_y, float cheek, float brow_y) {
  if (!g_fb) return;

  blink_l = clampf_emo(blink_l, 0.0f, 1.0f);
  blink_r = clampf_emo(blink_r, 0.0f, 1.0f);
  mouth_open = clampf_emo(mouth_open, 0.0f, 1.0f);
  cheek = clampf_emo(cheek, 0.0f, 1.0f);
  brow_y = clampf_emo(brow_y, -1.0f, 1.0f);
  look_x = clampf_emo(look_x, -1.0f, 1.0f);
  look_y = clampf_emo(look_y, -1.0f, 1.0f);

  float closed = (blink_l + blink_r) * 0.5f;
  uint8_t is_napping = (closed > 0.86f && look_y > 0.58f && mouth_open > 0.06f && mouth_open < 0.32f);
  uint8_t is_warm_or_dizzy = (mouth_open > 0.50f && closed > 0.28f && look_y > 0.12f);
  uint8_t is_tense = (brow_y < -0.50f && mouth_open < 0.18f);
  uint8_t is_searching = (brow_y > 0.62f && mouth_open < 0.22f && closed < 0.20f);
  uint8_t is_pout = (mouth_open > 0.48f && mouth_open < 0.68f &&
                     cheek > 0.62f && closed < 0.28f && brow_y < 0.28f);

  EMO_FillScreen(EMO_BLACK);

  // --- Square-screen friendly ambient shade ---
  // Avoid large circular rings; the physical screen is square, so a frame-like
  // shade looks cleaner and does not fight the panel shape.
  uint32_t glow = mix_rgb(is_napping ? 0x030408 : 0x03070D,
                          is_napping ? 0x101424 : 0x101A2A,
                          cheek * 0.45f + mouth_open * 0.12f + (is_napping ? 0.18f : 0.0f));
  if (cheek > 0.02f || mouth_open > 0.18f || is_napping) {
    EMO_FillRect(0, 0, g_w, 14, glow);
    EMO_FillRect(0, LCD_HEIGHT - 16, g_w, 16, glow);
    EMO_FillRect(0, 0, 10, LCD_HEIGHT, glow);
    EMO_FillRect(g_w - 10, 0, 10, LCD_HEIGHT, glow);
  }

  // --- Eyebrows ---
  int16_t brow_arc_r = 55;
  int16_t brow_thick = 3;
  int16_t brow_base_cy = EMO_LEFT_EYE_Y + 30;
  int16_t brow_dy = (int16_t)(-brow_y * 7.0f);
  float mood_tilt = look_x * 6.0f;

  EMO_DrawThickArc(EMO_LEFT_EYE_X, brow_base_cy + brow_dy + (int16_t)mood_tilt,
                   brow_arc_r, 68.0f, 112.0f, brow_thick, EMO_BROW);
  EMO_DrawThickArc(EMO_RIGHT_EYE_X, brow_base_cy + brow_dy - (int16_t)mood_tilt,
                   brow_arc_r, 68.0f, 112.0f, brow_thick, EMO_BROW);

  if (brow_y > 0.75f && mouth_open > 0.65f) {
    draw_sparkle(42, 48, 5, 0x9FD7FF);
    draw_sparkle(196, 50, 4, 0x9FD7FF);
  }

  // --- Eyes ---
  draw_eye(EMO_LEFT_EYE_X, EMO_LEFT_EYE_Y, blink_l, look_x, look_y);
  draw_eye(EMO_RIGHT_EYE_X, EMO_RIGHT_EYE_Y, blink_r, look_x, look_y);

  if (is_napping) {
    draw_z_mark(177, 36, 5, 0xAFC6FF);
    draw_z_mark(194, 24, 4, 0x6F86C8);
    EMO_FillCircle(EMO_MOUTH_CX + 27, EMO_MOUTH_CY - 3, 2, 0x536080);
    EMO_FillCircle(EMO_MOUTH_CX + 35, EMO_MOUTH_CY - 12, 1, 0x65749A);
  } else if (is_searching) {
    draw_sparkle(44, 42, 3, 0x7FCFFF);
    draw_sparkle(198, 44, 2, 0x7FCFFF);
  }

  if (is_tense) {
    draw_soft_line(49, 50, 42, 60, 1, 0x9A9A9A);
    draw_soft_line(56, 48, 50, 59, 1, 0x787878);
  }

  // --- Cheek blush ---
  if (cheek > 0.01f) {
    uint32_t bc = mix_rgb(0x22080C, 0xFF9FAF, cheek);
    int16_t cr = 7 + (int16_t)(cheek * 7.0f);
    int16_t cheek_y = EMO_LEFT_EYE_Y + EMO_EYE_R + 14;
    EMO_FillCircle(EMO_LEFT_EYE_X - EMO_EYE_R - 8, cheek_y, cr, bc);
    EMO_FillCircle(EMO_RIGHT_EYE_X + EMO_EYE_R + 8, cheek_y, cr, bc);
    EMO_DrawHLine(EMO_LEFT_EYE_X - EMO_EYE_R - 18, cheek_y - 3, 16, 1, 0xFFD1D8);
    EMO_DrawHLine(EMO_RIGHT_EYE_X + EMO_EYE_R + 2, cheek_y - 3, 16, 1, 0xFFD1D8);
  }

  if (is_warm_or_dizzy) {
    draw_sweat(191, 72, 5);
  }

  // --- Mouth ---
  int16_t mcx = EMO_MOUTH_CX;
  int16_t mcy = EMO_MOUTH_CY;
  int16_t mr = EMO_MOUTH_R;

  if (is_pout) {
    int16_t lip_y = (int16_t)(mcy + mr - 12);
    EMO_FillCircle((int16_t)(mcx - 5), lip_y, 6, 0xFFE0EA);
    EMO_FillCircle((int16_t)(mcx + 5), lip_y, 6, 0xFFE0EA);
    EMO_FillCircle(mcx, (int16_t)(lip_y + 1), 7, EMO_MOUTH);
    EMO_FillCircle(mcx, (int16_t)(lip_y + 1), 3, 0x0A0A12);
    EMO_FillCircle((int16_t)(mcx - 3), (int16_t)(lip_y - 3), 1, 0xFFFFFF);
  } else if (mouth_open < 0.15f) {
    if (brow_y < -0.42f) {
      // Frown for cold/annoyed states.
      EMO_DrawThickArc(mcx, mcy + 30, mr - 6, 30.0f, 150.0f, 3, EMO_MOUTH);
    } else {
      EMO_DrawThickArc(mcx, mcy, mr, 208.0f, 332.0f, 3, EMO_MOUTH);
      EMO_DrawThickArc(mcx, mcy + 1, mr - 4, 218.0f, 322.0f, 1, 0x6F7FA8);
    }
  } else if (mouth_open < 0.5f) {
    float t = (mouth_open - 0.15f) / 0.35f;
    int16_t adj_r = mr - (int16_t)(t * 7.0f);
    float half_span = 66.0f - t * 18.0f;
    EMO_DrawThickArc(mcx, mcy, adj_r, 270.0f - half_span, 270.0f + half_span, 4, EMO_MOUTH);
    if (t > 0.55f) {
      EMO_DrawThickArc(mcx, mcy + 2, adj_r - 4, 250.0f, 290.0f, 2, 0xFFE0E8);
    }
  } else {
    float t = (mouth_open - 0.5f) / 0.5f;
    int16_t om_w = 13 + (int16_t)(t * 9.0f);
    int16_t om_h = 20 + (int16_t)(t * 13.0f);
    int16_t om_y = mcy + mr - 17;
    int16_t om_x = mcx - om_w / 2;
    int16_t om_r = om_w / 2;
    EMO_FillCircle(om_x + om_r + 1, om_y + om_r + 2, om_r, 0x1E1E2A);
    EMO_FillCircle(om_x + om_r, om_y + om_r, om_r, EMO_MOUTH);
    EMO_FillCircle(om_x + om_r, om_y + om_h - om_r, om_r, EMO_MOUTH);
    EMO_FillRect(om_x, om_y + om_r, om_w, om_h - om_r * 2, EMO_MOUTH);
    EMO_FillCircle(mcx + 2, om_y + om_h - 7, om_r / 2, 0xFFC0CB);
  }
}
