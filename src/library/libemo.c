#include "include/libemo.h"
#include "include/lcd.h"
#include <math.h>
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

static uint16_t rgb565(uint32_t c) {
  return (uint16_t)(((c >> 19) << 11) | (((c >> 10) & 0x3F) << 5) | ((c >> 3) & 0x1F));
}

#define FB(x,y)  g_fb[(y) * g_w + (x)]

void EMO_Init(void) {
  g_fb = LCD_GetFrameBuffer();
  g_w  = LCD_GetWidth();
  g_h  = LCD_GetHeight();
}

void EMO_FillScreen(uint32_t color) {
  if (!g_fb) return;
  uint16_t c = rgb565(color);
  uint32_t n = (uint32_t)g_w * g_h;
  for (uint32_t i = 0; i < n; i++) g_fb[i] = c;
}

void EMO_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint32_t color) {
  if (!g_fb) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > g_w) w = g_w - x;
  if (y + h > g_h) h = g_h - y;
  if (w <= 0 || h <= 0) return;
  uint16_t c = rgb565(color);
  for (int16_t j = 0; j < h; j++)
    for (int16_t i = 0; i < w; i++)
      FB(x + i, y + j) = c;
}

void EMO_FillCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color) {
  if (!g_fb || r <= 0) return;
  uint16_t c = rgb565(color);
  int16_t r2 = r * r;
  for (int16_t dy = -r; dy <= r; dy++) {
    int16_t sy = cy + dy;
    if (sy < 0 || sy >= g_h) continue;
    int16_t dx = (int16_t)sqrtf((float)(r2 - dy * dy));
    int16_t x0 = cx - dx; if (x0 < 0) x0 = 0;
    int16_t x1 = cx + dx; if (x1 >= g_w) x1 = g_w - 1;
    for (int16_t sx = x0; sx <= x1; sx++) FB(sx, sy) = c;
  }
}

void EMO_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint32_t color) {
  if (!g_fb || r <= 0) return;
  uint16_t c = rgb565(color);
  int16_t x = r, y = 0, err = 0;
  while (x >= y) {
    FB(cx + x, cy + y) = c; FB(cx + y, cy + x) = c;
    FB(cx - y, cy + x) = c; FB(cx - x, cy + y) = c;
    FB(cx - x, cy - y) = c; FB(cx - y, cy - x) = c;
    FB(cx + y, cy - x) = c; FB(cx + x, cy - y) = c;
    if (err <= 0) { y++; err += 2 * y + 1; }
    if (err > 0)  { x--; err -= 2 * x + 1; }
  }
}

void EMO_DrawHLine(int16_t x, int16_t y, int16_t len, int16_t t, uint32_t color) {
  uint16_t c = rgb565(color);
  int16_t y0 = y - t / 2;
  for (int16_t dy = 0; dy < t; dy++) {
    int16_t sy = y0 + dy;
    if (sy < 0 || sy >= g_h) continue;
    for (int16_t dx = 0; dx < len; dx++) {
      int16_t sx = x + dx;
      if (sx >= 0 && sx < g_w) FB(sx, sy) = c;
    }
  }
}

void EMO_DrawThickArc(int16_t cx, int16_t cy, int16_t r, float s_deg, float e_deg, int16_t t, uint32_t color) {
  if (!g_fb || r <= 0 || t <= 0) return;
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

// ============ Eye drawing (with eyelid + pupil) ============

static void draw_eye(int16_t cx, int16_t cy, float blink, float lx, float ly) {
  int16_t r = EMO_EYE_R;
  if (blink < 0.0f) blink = 0.0f;
  if (blink > 1.0f) blink = 1.0f;

  // White sclera
  if (blink < 0.99f) {
    EMO_FillCircle(cx, cy, r, EMO_WHITE);
    // Pupil (black dot + highlight, only if eye is sufficiently open)
    if (blink < 0.7f) {
      int16_t px = cx + (int16_t)(lx * 6.0f);
      int16_t py = cy + (int16_t)(ly * 5.0f);
      int16_t max_off = r - EMO_PUPIL_R - 2;
      if (px < cx - max_off) px = cx - max_off;
      if (px > cx + max_off) px = cx + max_off;
      if (py < cy - max_off) py = cy - max_off;
      if (py > cy + max_off) py = cy + max_off;
      EMO_FillCircle(px, py, EMO_PUPIL_R, EMO_BLACK);
      EMO_FillCircle(px - 2, py - 2, 2, EMO_WHITE);
    }
    // Eyelid cover — drawn LAST so it covers pupil too
    int16_t cover_h = (int16_t)(blink * r * 2.2f);
    if (cover_h > 0) {
      EMO_FillRect(cx - r - 2, cy - r - 1, r * 2 + 4, cover_h, EMO_BLACK);
    }
  } else {
    // Fully closed: thin horizontal line spanning eye width
    EMO_DrawHLine(cx - r, cy + r, r * 2, 2, EMO_WHITE);
  }
}

// ============ Main face ============

void EMO_DrawFace(float blink_l, float blink_r, float mouth_open,
                  float look_x, float look_y, float cheek, float brow_y) {
  if (!g_fb) return;

  // Clamp
  #define CLAMP(v,lo,hi) do{if((v)<(lo))(v)=(lo);if((v)>(hi))(v)=(hi);}while(0)
  CLAMP(blink_l, 0, 1); CLAMP(blink_r, 0, 1);
  CLAMP(mouth_open, 0, 1); CLAMP(cheek, 0, 1); CLAMP(brow_y, -1, 1);
  CLAMP(look_x, -1, 1); CLAMP(look_y, -1, 1);

  EMO_FillScreen(EMO_BLACK);

  // --- Eyebrows (shallow upward arcs) ---
  // Arc centre sits below eye; large radius produces a gentle curve above the eye.
  int16_t brow_arc_r   = 55;                        // large radius → shallow curve
  int16_t brow_thick   = 3;
  int16_t brow_base_cy = EMO_LEFT_EYE_Y + 30;       // arc centre below eye
  int16_t brow_dy = (int16_t)(-brow_y * 6.0f);      // raise = shift centre up (lower y)

  // Left brow: arc through top of circle (70°→110°), centred above left eye
  {
    int16_t cy = brow_base_cy + brow_dy;
    EMO_DrawThickArc(EMO_LEFT_EYE_X, cy, brow_arc_r, 70.0f, 110.0f, brow_thick, EMO_BROW);
  }
  // Right brow: same arc, centred above right eye
  {
    int16_t cy = brow_base_cy + brow_dy;
    EMO_DrawThickArc(EMO_RIGHT_EYE_X, cy, brow_arc_r, 70.0f, 110.0f, brow_thick, EMO_BROW);
  }

  // --- Eyes ---
  draw_eye(EMO_LEFT_EYE_X,  EMO_LEFT_EYE_Y,  blink_l, look_x, look_y);
  draw_eye(EMO_RIGHT_EYE_X, EMO_RIGHT_EYE_Y, blink_r, look_x, look_y);

  // --- Cheek blush (outside & below eyes) ---
  if (cheek > 0.01f) {
    uint32_t blush = 0xFFAAAA;
    uint8_t rr = (uint8_t)(((blush >> 16) & 0xFF) * cheek);
    uint8_t gg = (uint8_t)(((blush >> 8)  & 0xFF) * cheek * 0.5f);
    uint8_t bb = (uint8_t)(( blush        & 0xFF) * cheek * 0.5f);
    uint32_t bc = ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | bb;
    int16_t cr = 8 + (int16_t)(cheek * 5.0f);
    int16_t cheek_y = EMO_LEFT_EYE_Y + EMO_EYE_R + 14;
    // Outer side of each eye
    EMO_FillCircle(EMO_LEFT_EYE_X  - EMO_EYE_R - 6, cheek_y, cr, bc);
    EMO_FillCircle(EMO_RIGHT_EYE_X + EMO_EYE_R + 6, cheek_y, cr, bc);
  }

  // --- Mouth ---
  int16_t mcx = EMO_MOUTH_CX;
  int16_t mcy = EMO_MOUTH_CY;
  int16_t mr  = EMO_MOUTH_R;

  if (mouth_open < 0.15f) {
    // Neutral: thin clean arc smile
    EMO_DrawThickArc(mcx, mcy, mr, 208.0f, 332.0f, 3, EMO_MOUTH);
  } else if (mouth_open < 0.5f) {
    // Happy: wider arc, smoothly transitioning
    float t = (mouth_open - 0.15f) / 0.35f;
    int16_t adj_r = mr - (int16_t)(t * 5.0f);
    float half_span = 64.0f - t * 16.0f;
    EMO_DrawThickArc(mcx, mcy, adj_r, 270.0f - half_span, 270.0f + half_span, 3, EMO_MOUTH);
  } else {
    // Surprised: tall pill shape (vertical rounded rect)
    float t = (mouth_open - 0.5f) / 0.5f;
    int16_t om_w = 14 + (int16_t)(t * 6.0f);   // width 14~20
    int16_t om_h = 20 + (int16_t)(t * 10.0f);  // height 20~30 (taller than wide)
    int16_t om_y = mcy + mr - 16;
    int16_t om_x = mcx - om_w / 2;
    int16_t om_r = om_w / 2;                   // end-cap radius
    EMO_FillCircle(om_x + om_r, om_y + om_r, om_r, EMO_MOUTH);
    EMO_FillCircle(om_x + om_r, om_y + om_h - om_r, om_r, EMO_MOUTH);
    EMO_FillRect(om_x, om_y + om_r, om_w, om_h - om_r * 2, EMO_MOUTH);
  }
}
