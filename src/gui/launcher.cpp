#include "include/launcher.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libehw.h"
#include "library/include/libemo.h"
#include <cmath>
#include <cstdio>

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern KeyManager keyManager;
extern LCD boardLCD;

// ============ Animation state (CCMRAM to save main RAM) ============
static CCMRAM float pet_blink_l  = 0.0f;
static CCMRAM float pet_blink_r  = 0.0f;
static CCMRAM float pet_mouth    = 0.0f;
static CCMRAM float pet_look_x   = 0.0f;
static CCMRAM float pet_look_y   = 0.0f;
static CCMRAM float pet_cheek    = 0.0f;
static CCMRAM float pet_brow_y   = 0.0f;

enum PetAnim {
  ANIM_IDLE = 0,
  ANIM_BLINK,
  ANIM_WINK,
  ANIM_DBLINK,
  ANIM_HAPPY,
  ANIM_SURPRISED,
  ANIM_CURIOUS,
  // Sensor-driven expressions
  ANIM_DIZZY,
  ANIM_PETTED,
  ANIM_COLD,
  ANIM_COMFY,
  ANIM_HOT,
  ANIM_DARK,
  ANIM_BRIGHT,
};
static CCMRAM int  pet_state    = ANIM_IDLE;
static CCMRAM int  blink_phase  = 0;

static CCMRAM uint32_t next_blink_tm  = 0;
static CCMRAM uint32_t anim_start_tm  = 0;
static CCMRAM uint32_t mood_timer     = 0;
static CCMRAM uint32_t sensor_timer    = 0;
static CCMRAM uint32_t sensor_start    = 0;  // when current sensor expr began
static CCMRAM int  prev_pet_state      = ANIM_IDLE;

#define SENSOR_MAX_HOLD  8000    // force back to idle after 8s
#define SENSOR_REST_MS   18000   // rest period before sensor can re-trigger

// Triple-press ENTER to exit
static CCMRAM uint32_t enter_tm[3]  = {0};
static CCMRAM int      enter_idx    = 0;

#define TRIPLE_WINDOW   800   // 3 presses within 800ms
#define BLINK_DUR_MS    420
#define WINK_DUR_MS     380
#define DBLINK_DUR_MS   700
#define SENSOR_POLL_MS  500

static uint32_t rnd(uint32_t max) {
  static uint32_t seed = 0xBEEF;
  seed = seed * 1103515245 + 12345;
  return seed % max;
}

// ============ Easing ============

static float ease_inout(float t) {
  return t < 0.5f ? 2*t*t : 1 - (-2*t+2)*(-2*t+2)/2;
}

// ============ Sensor → expression mapping ============

static int expr_to_anim(EHW_Expr_t e) {
  switch (e) {
  case EHW_EXPR_DIZZY:  return ANIM_DIZZY;
  case EHW_EXPR_PETTED: return ANIM_PETTED;
  case EHW_EXPR_COLD:   return ANIM_COLD;
  case EHW_EXPR_COMFY:  return ANIM_COMFY;
  case EHW_EXPR_HOT:    return ANIM_HOT;
  case EHW_EXPR_DARK:   return ANIM_DARK;
  case EHW_EXPR_BRIGHT: return ANIM_BRIGHT;
  default:              return -1;
  }
}

// ============ Per-frame update ============

static void update_idle_motion(void) {
  uint32_t now = HAL_GetTick();
  if (pet_state != ANIM_IDLE) return;

  float lt = (float)(now % 4500) / 4500.0f;
  float phase = lt * 2.0f * 3.14159f;
  pet_look_x = sinf(phase) * 0.55f;
  pet_look_y = cosf(phase * 1.3f) * 0.25f;
  pet_brow_y = sinf(phase * 0.7f) * 0.2f;
}

static void update_sensor(void) {
  uint32_t now = HAL_GetTick();
  if (now - sensor_timer < SENSOR_POLL_MS) return;
  sensor_timer = now;

  // Timeout: force sensor expression to release after too long
  if (pet_state >= ANIM_DIZZY && pet_state <= ANIM_BRIGHT) {
    if (now - sensor_start > SENSOR_MAX_HOLD) {
      pet_state = ANIM_IDLE;
      prev_pet_state = ANIM_IDLE;
      sensor_start = now + SENSOR_REST_MS;  // block sensor for rest period
      pet_mouth = 0.0f; pet_cheek = 0.0f;
      pet_blink_l = 0.0f; pet_blink_r = 0.0f;
      pet_brow_y = 0.0f; pet_look_x = 0.0f; pet_look_y = 0.0f;
      return;
    }
  }

  // Don't poll sensors during rest period
  if (now < sensor_start) return;

  EHW_Expr_t e = EHW_Update();
  int anim = expr_to_anim(e);
  if (anim < 0) {
    if (pet_state >= ANIM_DIZZY && pet_state <= ANIM_BRIGHT) {
      pet_state = prev_pet_state;
      prev_pet_state = ANIM_IDLE;
      sensor_start = now + SENSOR_REST_MS;  // rest before re-trigger
      pet_mouth = 0.0f; pet_cheek = 0.0f;
      pet_blink_l = 0.0f; pet_blink_r = 0.0f;
      pet_brow_y = 0.0f; pet_look_x = 0.0f; pet_look_y = 0.0f;
    }
    return;
  }

  if (pet_state != anim) {
    if (pet_state < ANIM_DIZZY) prev_pet_state = pet_state;
    pet_state = anim;
    sensor_start = now;
  }
}

static void update_animation(void) {
  uint32_t now = HAL_GetTick();

  update_idle_motion();
  update_sensor();

  switch (pet_state) {

  case ANIM_IDLE:
    pet_mouth   = 0.0f;
    pet_blink_l = 0.0f;
    pet_blink_r = 0.0f;
    pet_cheek   = 0.0f;

    if (now >= next_blink_tm) {
      int r = rnd(10);
      if (r < 2)      { pet_state = ANIM_WINK; anim_start_tm = now; blink_phase = 0; }
      else if (r < 4) { pet_state = ANIM_DBLINK; anim_start_tm = now; blink_phase = 0; }
      else            { pet_state = ANIM_BLINK; anim_start_tm = now; blink_phase = 0; }
    }

    if (now - mood_timer > 8000 + rnd(7000)) {
      mood_timer = now;
      int r = rnd(10);
      if (r < 2)      pet_state = ANIM_SURPRISED;
      else if (r < 5) pet_state = ANIM_HAPPY;
      else            pet_state = ANIM_CURIOUS;
      anim_start_tm = now;
    }
    break;

  case ANIM_BLINK:
    {
      int elapsed = (int)(now - anim_start_tm);
      int half = BLINK_DUR_MS / 2;
      if (blink_phase == 0) {
        if (elapsed < half) {
          float t = (float)elapsed / half;
          pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
        } else { pet_blink_l = 1.0f; pet_blink_r = 1.0f; blink_phase = 1; }
      } else {
        int t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / half;
          pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE; next_blink_tm = now + 2500 + rnd(3500);
        }
      }
    }
    break;

  case ANIM_WINK:
    {
      int elapsed = (int)(now - anim_start_tm);
      int half = WINK_DUR_MS / 2;
      if (blink_phase == 0) {
        if (elapsed < half) {
          float t = (float)elapsed / half;
          pet_blink_l = ease_inout(t); pet_blink_r = 0.0f;
        } else { pet_blink_l = 1.0f; blink_phase = 1; }
      } else {
        int t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / half;
          pet_blink_l = 1.0f - ease_inout(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE; next_blink_tm = now + 2500 + rnd(3000);
        }
      }
    }
    break;

  case ANIM_DBLINK:
    {
      int elapsed = (int)(now - anim_start_tm);
      int seg = DBLINK_DUR_MS / 4;
      if (elapsed < seg) {
        float t = (float)elapsed / seg;
        pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
      } else if (elapsed < seg * 2) {
        float t = (float)(elapsed - seg) / seg;
        pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
      } else if (elapsed < seg * 3) {
        float t = (float)(elapsed - seg * 2) / seg;
        pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
      } else if (elapsed < seg * 4) {
        float t = (float)(elapsed - seg * 3) / seg;
        pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
      } else {
        pet_blink_l = 0.0f; pet_blink_r = 0.0f;
        pet_state = ANIM_IDLE; next_blink_tm = now + 2500 + rnd(3000);
      }
    }
    break;

  case ANIM_HAPPY:
    {
      int elapsed = (int)(now - anim_start_tm);
      if (elapsed < 300) {
        float t = (float)elapsed / 300.0f;
        pet_mouth = t * 0.35f; pet_cheek = t; pet_brow_y = t * 0.4f;
      } else if (elapsed < 1800) {
        pet_mouth = 0.35f; pet_cheek = 1.0f; pet_brow_y = 0.4f;
      } else if (elapsed < 2200) {
        float t = (float)(elapsed - 1800) / 400.0f;
        pet_mouth = 0.35f * (1.0f - t); pet_cheek = 1.0f - t; pet_brow_y = 0.4f * (1.0f - t);
      } else {
        pet_mouth = 0.0f; pet_cheek = 0.0f; pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_SURPRISED:
    {
      int elapsed = (int)(now - anim_start_tm);
      if (elapsed < 180) {
        float t = (float)elapsed / 180.0f;
        pet_mouth = 0.5f + t * 0.5f; pet_brow_y = t * 1.0f;
      } else if (elapsed < 1200) {
        pet_mouth = 1.0f; pet_brow_y = 1.0f;
      } else if (elapsed < 1500) {
        float t = (float)(elapsed - 1200) / 300.0f;
        pet_mouth = 1.0f * (1.0f - t); pet_brow_y = 1.0f * (1.0f - t);
      } else {
        pet_mouth = 0.0f; pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_CURIOUS:
    {
      int elapsed = (int)(now - anim_start_tm);
      if (elapsed < 400) {
        float t = (float)elapsed / 400.0f;
        pet_brow_y = t * 0.55f; pet_mouth = t * 0.18f; pet_look_x = t * 0.7f;
      } else if (elapsed < 2000) {
        pet_brow_y = 0.55f; pet_mouth = 0.18f; pet_look_x = 0.7f;
      } else if (elapsed < 2600) {
        float t = (float)(elapsed - 2000) / 600.0f;
        pet_brow_y = 0.55f * (1.0f - t); pet_mouth = 0.18f * (1.0f - t); pet_look_x = 0.7f * (1.0f - t);
      } else {
        pet_brow_y = 0.0f; pet_mouth = 0.0f; pet_look_x = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  // --- Sensor-driven expressions (hold while sensor active) ---

  case ANIM_DIZZY:
    // Cross-eyed, wavy mouth — slow oscillation
    {
      float t = (float)(now % 800) / 800.0f;
      pet_mouth   = 0.6f + sinf(t * 6.28f * 1.2f) * 0.12f;
      pet_blink_l = (sinf(t * 6.28f * 2.0f) > 0.2f) ? 0.0f : 0.5f;
      pet_blink_r = (sinf(t * 6.28f * 2.0f + 1.5f) > 0.2f) ? 0.0f : 0.5f;
      pet_look_x  = sinf(t * 6.28f * 1.3f) * 0.7f;
      pet_look_y  = cosf(t * 6.28f * 1.6f) * 0.5f;
      pet_brow_y  = -0.2f + sinf(t * 6.28f * 1.0f) * 0.4f;
      pet_cheek   = 0.0f;
    }
    break;

  case ANIM_PETTED:
    {
      float br = (float)(now % 4200) / 4200.0f * 6.28f;
      pet_blink_l = 0.25f + sinf(br * 1.3f) * 0.08f;
      pet_blink_r = 0.25f + sinf(br * 1.5f) * 0.08f;
      pet_mouth   = 0.30f;
      pet_cheek   = 0.9f;
      pet_brow_y  = 0.5f + sinf(br * 0.7f) * 0.15f;
      pet_look_x  = sinf(br * 0.7f) * 0.5f;
      pet_look_y  = -0.2f + cosf(br * 0.9f) * 0.25f;
    }
    break;

  case ANIM_COLD:
    {
      float shiver = sinf((float)(now % 500) / 500.0f * 6.28f * 2.2f);
      pet_blink_l = 0.08f + shiver * 0.15f;
      pet_blink_r = 0.08f + shiver * 0.15f;
      pet_mouth   = 0.0f;
      pet_cheek   = 0.0f;
      pet_brow_y  = -0.5f + shiver * 0.15f;
      pet_look_x  = shiver * 0.3f;
      pet_look_y  = 0.05f;
    }
    break;

  case ANIM_COMFY:
    {
      float br = (float)(now % 5000) / 5000.0f * 6.28f;
      pet_blink_l = 0.12f + sinf(br * 1.3f) * 0.06f;
      pet_blink_r = 0.12f + sinf(br * 1.5f) * 0.06f;
      pet_mouth   = 0.18f;
      pet_cheek   = 0.6f;
      pet_brow_y  = 0.3f + sinf(br * 0.7f) * 0.15f;
      pet_look_x  = sinf(br * 0.5f) * 0.45f;
      pet_look_y  = cosf(br * 0.7f) * 0.25f;
    }
    break;

  case ANIM_HOT:
    {
      float br = (float)(now % 3500) / 3500.0f * 6.28f;
      float pant = sinf((float)(now % 350) / 350.0f * 6.28f);
      pet_blink_l = 0.35f + sinf(br * 1.2f) * 0.05f;
      pet_blink_r = 0.35f + sinf(br * 1.4f) * 0.05f;
      pet_mouth   = 0.55f + pant * 0.15f;
      pet_cheek   = 0.3f;
      pet_brow_y  = 0.4f;
      pet_look_x  = sinf(br * 0.5f) * 0.3f;
      pet_look_y  = 0.3f + cosf(br * 0.6f) * 0.15f;
    }
    break;

  case ANIM_DARK:
    {
      float br = (float)(now % 3800) / 3800.0f * 6.28f;
      pet_blink_l = sinf(br * 1.4f) * 0.06f;
      pet_blink_r = sinf(br * 1.3f) * 0.06f;
      pet_mouth   = 0.05f;
      pet_cheek   = 0.0f;
      pet_brow_y  = 0.7f + sinf(br * 0.5f) * 0.12f;
      pet_look_x  = sinf(br * 0.6f) * 0.55f;
      pet_look_y  = 0.1f + cosf(br * 0.7f) * 0.25f;
    }
    break;

  case ANIM_BRIGHT:
    {
      float br = (float)(now % 4500) / 4500.0f * 6.28f;
      pet_blink_l = 0.45f + sinf(br * 1.2f) * 0.06f;
      pet_blink_r = 0.45f + sinf(br * 1.4f) * 0.06f;
      pet_mouth   = 0.0f;
      pet_cheek   = 0.0f;
      pet_brow_y  = -0.5f + sinf(br * 0.5f) * 0.1f;
      pet_look_x  = sinf(br * 0.4f) * 0.3f;
      pet_look_y  = 0.15f + cosf(br * 0.5f) * 0.1f;
    }
    break;
  }

  if (pet_state != ANIM_HAPPY && pet_state != ANIM_PETTED && pet_state != ANIM_COMFY
      && pet_cheek > 0.0f) {
    pet_cheek -= 0.03f;
    if (pet_cheek < 0.0f) pet_cheek = 0.0f;
  }
}

// ============ Main loop ============

void pet_launcher_run(void) {
  EMO_Init();
  EHW_Init();

  next_blink_tm = HAL_GetTick() + 2500 + rnd(3000);
  mood_timer    = HAL_GetTick() + 8000 + rnd(5000);
  sensor_timer  = 0;
  sensor_start  = 0;
  enter_idx    = 0;
  enter_tm[0] = enter_tm[1] = enter_tm[2] = 0;

  pet_blink_l = 0.0f; pet_blink_r = 0.0f;
  pet_mouth   = 0.0f; pet_cheek   = 0.0f;
  pet_look_x  = 0.0f; pet_look_y  = 0.0f;
  pet_brow_y  = 0.0f;
  pet_state   = ANIM_IDLE;

  printf("[Pet] Launcher started — triple-press ENTER to exit\r\n");

  uint32_t heartbeat = 0;
  while (1) {
    uint32_t now = HAL_GetTick();
    if (now - heartbeat > 5000) {
      heartbeat = now;
      printf("[Pet] alive @ %lums, state=%d, expr=%d\r\n", now, pet_state, EHW_GetExpr());
    }

    // --- Input: triple-press ENTER to exit ---
    keyManager.btn_enter.tick();
    if (keyManager.btn_enter.getState() == KEY_PRESSED) {
      enter_tm[enter_idx % 3] = now;
      enter_idx++;
      // Check if last 3 presses within TRIPLE_WINDOW
      if (enter_idx >= 3) {
        uint32_t t0 = enter_tm[(enter_idx - 3) % 3];
        uint32_t t2 = enter_tm[(enter_idx - 1) % 3];
        if (t2 - t0 < TRIPLE_WINDOW) {
          printf("[Pet] Triple ENTER — returning to menu\r\n");
          EMO_FillScreen(EMO_BLACK);
          LCD_Flush();
          return;
        }
      }
    }

    // --- Update & Draw ---
    boardLCD.updateAutoBrightness();
    update_animation();
    EMO_DrawFace(pet_blink_l, pet_blink_r, pet_mouth,
                 pet_look_x, pet_look_y, pet_cheek, pet_brow_y);
    LCD_Flush();
    HAL_Delay(10);
  }
}
