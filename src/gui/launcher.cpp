#include "include/launcher.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libehw.h"
#include "library/include/libemo.h"
#include <cmath>
#include <cstdio>
#include "syslog.h"

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

extern KeyManager keyManager;
extern LCD boardLCD;

// ============ Animation state (CCMRAM to save main RAM) ============
static CCMRAM float pet_blink_l = 0.0f;
static CCMRAM float pet_blink_r = 0.0f;
static CCMRAM float pet_mouth   = 0.0f;
static CCMRAM float pet_look_x  = 0.0f;
static CCMRAM float pet_look_y  = 0.0f;
static CCMRAM float pet_cheek   = 0.0f;
static CCMRAM float pet_brow_y  = 0.0f;

static CCMRAM float pet_look_tx = 0.0f;
static CCMRAM float pet_look_ty = 0.0f;
static CCMRAM uint32_t next_saccade_tm = 0;

enum PetAnim {
  ANIM_IDLE = 0,
  ANIM_BLINK,
  ANIM_WINK,
  ANIM_DBLINK,
  ANIM_HAPPY,
  ANIM_SURPRISED,
  ANIM_CURIOUS,
  ANIM_SLEEPY,
  ANIM_SHY,
  ANIM_PROUD,
  ANIM_ANNOYED,
  // Sensor-driven expressions
  ANIM_DIZZY,
  ANIM_PETTED,
  ANIM_COLD,
  ANIM_COMFY,
  ANIM_HOT,
  ANIM_DARK,
  ANIM_BRIGHT,
};

static CCMRAM int pet_state = ANIM_IDLE;
static CCMRAM int blink_phase = 0;

static CCMRAM uint32_t next_blink_tm = 0;
static CCMRAM uint32_t anim_start_tm = 0;
static CCMRAM uint32_t mood_timer = 0;
static CCMRAM uint32_t sensor_timer = 0;
static CCMRAM uint32_t sensor_expr_start = 0;
static CCMRAM uint32_t sensor_cooldown_until = 0;
static CCMRAM int prev_pet_state = ANIM_IDLE;

#define SENSOR_MAX_HOLD  5200U
#define SENSOR_REST_MS   180U
#define SENSOR_POLL_MS   90U

// Triple-press ENTER to exit
static CCMRAM uint32_t enter_tm[3] = {0};
static CCMRAM int enter_idx = 0;
static CCMRAM uint8_t last_enter_pressed = 0;

#define TRIPLE_WINDOW   800U
#define BLINK_DUR_MS    420U
#define WINK_DUR_MS     380U
#define DBLINK_DUR_MS   700U

static uint32_t rnd(uint32_t max) {
  static uint32_t seed = 0xBEEF;
  seed = seed * 1103515245U + 12345U;
  return max ? (seed % max) : 0U;
}

// ============ Easing ============

static float ease_inout(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
}

static float lerp(float a, float b, float k) {
  return a + (b - a) * k;
}

static bool is_sensor_anim(int s) {
  return s >= ANIM_DIZZY && s <= ANIM_BRIGHT;
}

static bool is_motion_anim(int s) {
  return s == ANIM_DIZZY || s == ANIM_PETTED;
}

static void reset_pose_soft(void) {
  pet_mouth = lerp(pet_mouth, 0.0f, 0.24f);
  pet_cheek = lerp(pet_cheek, 0.0f, 0.08f);
  pet_blink_l = lerp(pet_blink_l, 0.0f, 0.18f);
  pet_blink_r = lerp(pet_blink_r, 0.0f, 0.18f);
  pet_brow_y = lerp(pet_brow_y, 0.0f, 0.16f);
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

  if (now >= next_saccade_tm) {
    next_saccade_tm = now + 850U + rnd(2200U);
    pet_look_tx = ((int32_t)rnd(2001U) - 1000) / 1000.0f * 0.72f;
    pet_look_ty = ((int32_t)rnd(1601U) - 800) / 1000.0f * 0.36f;
  }

  float lt = (float)(now % 5200U) / 5200.0f;
  float phase = lt * 2.0f * 3.14159f;
  float breath = sinf(phase);

  pet_look_x = lerp(pet_look_x, pet_look_tx + sinf(phase * 0.45f) * 0.10f, 0.045f);
  pet_look_y = lerp(pet_look_y, pet_look_ty + cosf(phase * 0.63f) * 0.06f, 0.045f);
  pet_brow_y = lerp(pet_brow_y, breath * 0.13f, 0.055f);
  pet_mouth = lerp(pet_mouth, 0.025f + (breath + 1.0f) * 0.018f, 0.08f);
}

static void release_sensor_expression(uint32_t now) {
  int was_motion = is_motion_anim(pet_state);
  pet_state = ANIM_IDLE;
  prev_pet_state = ANIM_IDLE;
  // Keep motion/gravity highly responsive.  The old multi-second rest period
  // made random expressions look like they were stealing sensor events.
  sensor_cooldown_until = now + (was_motion ? SENSOR_REST_MS : 900U);
  reset_pose_soft();
}

static void update_sensor(void) {
  uint32_t now = HAL_GetTick();
  if (now - sensor_timer < SENSOR_POLL_MS) return;
  sensor_timer = now;

  if (is_sensor_anim(pet_state) && now - sensor_expr_start > SENSOR_MAX_HOLD) {
    release_sensor_expression(now);
    return;
  }

  EHW_Expr_t e = EHW_Update();
  int anim = expr_to_anim(e);

  if (anim < 0) {
    if (is_sensor_anim(pet_state)) {
      release_sensor_expression(now);
    }
    return;
  }

  // Motion/gravity events are high priority and can preempt any random mood.
  // Slow environment moods still respect a short cooldown to avoid flicker.
  if (now < sensor_cooldown_until && !is_motion_anim(anim)) return;

  if (pet_state != anim) {
    if (!is_sensor_anim(pet_state)) prev_pet_state = pet_state;
    pet_state = anim;
    sensor_expr_start = now;
    anim_start_tm = now;
    blink_phase = 0;
  }
}

static void update_animation(void) {
  uint32_t now = HAL_GetTick();

  update_idle_motion();
  update_sensor();

  switch (pet_state) {

  case ANIM_IDLE:
    if (pet_cheek > 0.0f) pet_cheek = lerp(pet_cheek, 0.0f, 0.06f);
    if (now >= next_blink_tm) {
      int r = rnd(12U);
      if (r < 2)      { pet_state = ANIM_WINK; anim_start_tm = now; blink_phase = 0; }
      else if (r < 4) { pet_state = ANIM_DBLINK; anim_start_tm = now; blink_phase = 0; }
      else            { pet_state = ANIM_BLINK; anim_start_tm = now; blink_phase = 0; }
    }

    // Do not start a random mood while the sensor engine has an active event.
    if (EHW_GetExpr() == EHW_EXPR_NONE && now - mood_timer > 6200U + rnd(7600U)) {
      mood_timer = now;
      int r = rnd(16U);
      if (r < 2)       pet_state = ANIM_SURPRISED;
      else if (r < 5)  pet_state = ANIM_HAPPY;
      else if (r < 8)  pet_state = ANIM_CURIOUS;
      else if (r < 10) pet_state = ANIM_SLEEPY;
      else if (r < 12) pet_state = ANIM_SHY;
      else if (r < 14) pet_state = ANIM_PROUD;
      else             pet_state = ANIM_ANNOYED;
      anim_start_tm = now;
    }
    break;

  case ANIM_BLINK:
    {
      uint32_t elapsed = now - anim_start_tm;
      uint32_t half = BLINK_DUR_MS / 2U;
      if (blink_phase == 0) {
        if (elapsed < half) {
          float t = (float)elapsed / (float)half;
          pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
        } else { pet_blink_l = 1.0f; pet_blink_r = 1.0f; blink_phase = 1; }
      } else {
        uint32_t t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / (float)half;
          pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE; next_blink_tm = now + 2300U + rnd(3600U);
        }
      }
    }
    break;

  case ANIM_WINK:
    {
      uint32_t elapsed = now - anim_start_tm;
      uint32_t half = WINK_DUR_MS / 2U;
      pet_cheek = lerp(pet_cheek, 0.35f, 0.10f);
      if (blink_phase == 0) {
        if (elapsed < half) {
          float t = (float)elapsed / (float)half;
          pet_blink_l = ease_inout(t); pet_blink_r = 0.0f;
        } else { pet_blink_l = 1.0f; blink_phase = 1; }
      } else {
        uint32_t t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / (float)half;
          pet_blink_l = 1.0f - ease_inout(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE; next_blink_tm = now + 2400U + rnd(3000U);
        }
      }
    }
    break;

  case ANIM_DBLINK:
    {
      uint32_t elapsed = now - anim_start_tm;
      uint32_t seg = DBLINK_DUR_MS / 4U;
      if (elapsed < seg) {
        float t = (float)elapsed / (float)seg;
        pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
      } else if (elapsed < seg * 2U) {
        float t = (float)(elapsed - seg) / (float)seg;
        pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
      } else if (elapsed < seg * 3U) {
        float t = (float)(elapsed - seg * 2U) / (float)seg;
        pet_blink_l = ease_inout(t); pet_blink_r = ease_inout(t);
      } else if (elapsed < seg * 4U) {
        float t = (float)(elapsed - seg * 3U) / (float)seg;
        pet_blink_l = 1.0f - ease_inout(t); pet_blink_r = 1.0f - ease_inout(t);
      } else {
        pet_blink_l = 0.0f; pet_blink_r = 0.0f;
        pet_state = ANIM_IDLE; next_blink_tm = now + 2300U + rnd(3000U);
      }
    }
    break;

  case ANIM_HAPPY:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (elapsed < 300U) {
        float t = (float)elapsed / 300.0f;
        pet_mouth = lerp(pet_mouth, 0.42f, t); pet_cheek = t; pet_brow_y = t * 0.52f;
      } else if (elapsed < 1800U) {
        float wave = sinf((float)(now % 800U) / 800.0f * 6.28318f);
        pet_mouth = 0.42f + wave * 0.035f; pet_cheek = 1.0f; pet_brow_y = 0.42f + wave * 0.08f;
      } else if (elapsed < 2300U) {
        float t = (float)(elapsed - 1800U) / 500.0f;
        pet_mouth = 0.42f * (1.0f - t); pet_cheek = 1.0f - t; pet_brow_y = 0.42f * (1.0f - t);
      } else {
        pet_mouth = 0.0f; pet_cheek = 0.0f; pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_SURPRISED:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (elapsed < 180U) {
        float t = (float)elapsed / 180.0f;
        pet_mouth = 0.45f + t * 0.55f; pet_brow_y = t;
        pet_look_x = 0.0f; pet_look_y = -0.15f;
      } else if (elapsed < 1150U) {
        pet_mouth = 1.0f; pet_brow_y = 1.0f; pet_blink_l = 0.0f; pet_blink_r = 0.0f;
      } else if (elapsed < 1550U) {
        float t = (float)(elapsed - 1150U) / 400.0f;
        pet_mouth = 1.0f - t; pet_brow_y = 1.0f - t;
      } else {
        pet_mouth = 0.0f; pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_CURIOUS:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (elapsed < 450U) {
        float t = (float)elapsed / 450.0f;
        pet_brow_y = t * 0.62f; pet_mouth = t * 0.16f; pet_look_x = t * 0.82f; pet_look_y = -t * 0.18f;
      } else if (elapsed < 2050U) {
        float n = sinf((float)(now % 900U) / 900.0f * 6.28318f);
        pet_brow_y = 0.60f; pet_mouth = 0.16f; pet_look_x = 0.75f + n * 0.08f; pet_look_y = -0.16f;
      } else if (elapsed < 2650U) {
        float t = (float)(elapsed - 2050U) / 600.0f;
        pet_brow_y = 0.60f * (1.0f - t); pet_mouth = 0.16f * (1.0f - t); pet_look_x = 0.75f * (1.0f - t);
      } else {
        pet_brow_y = 0.0f; pet_mouth = 0.0f; pet_look_x = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_SLEEPY:
    {
      uint32_t elapsed = now - anim_start_tm;
      float br = (float)(now % 2400U) / 2400.0f * 6.28318f;
      pet_blink_l = 0.54f + sinf(br) * 0.18f;
      pet_blink_r = 0.54f + sinf(br + 0.2f) * 0.18f;
      pet_mouth = 0.08f + (sinf(br * 0.5f) + 1.0f) * 0.035f;
      pet_brow_y = -0.18f;
      pet_look_x = lerp(pet_look_x, 0.0f, 0.06f);
      pet_look_y = lerp(pet_look_y, 0.22f, 0.06f);
      if (elapsed > 2600U) {
        pet_state = ANIM_IDLE; pet_blink_l = 0.0f; pet_blink_r = 0.0f;
      }
    }
    break;

  case ANIM_SHY:
    {
      uint32_t elapsed = now - anim_start_tm;
      float t = elapsed < 500U ? (float)elapsed / 500.0f : 1.0f;
      pet_cheek = lerp(pet_cheek, 0.95f, 0.12f);
      pet_blink_l = 0.15f + sinf((float)(now % 1600U) / 1600.0f * 6.28318f) * 0.08f;
      pet_blink_r = 0.15f + sinf((float)(now % 1500U) / 1500.0f * 6.28318f) * 0.08f;
      pet_mouth = 0.20f * t;
      pet_brow_y = 0.25f * t;
      pet_look_x = lerp(pet_look_x, -0.55f, 0.08f);
      pet_look_y = lerp(pet_look_y, 0.18f, 0.08f);
      if (elapsed > 2400U) pet_state = ANIM_IDLE;
    }
    break;

  case ANIM_PROUD:
    {
      uint32_t elapsed = now - anim_start_tm;
      pet_blink_l = lerp(pet_blink_l, 0.06f, 0.08f);
      pet_blink_r = lerp(pet_blink_r, 0.06f, 0.08f);
      pet_mouth = lerp(pet_mouth, 0.30f, 0.10f);
      pet_cheek = lerp(pet_cheek, 0.45f, 0.06f);
      pet_brow_y = lerp(pet_brow_y, 0.56f, 0.08f);
      pet_look_x = lerp(pet_look_x, 0.35f, 0.07f);
      pet_look_y = lerp(pet_look_y, -0.20f, 0.07f);
      if (elapsed > 2100U) pet_state = ANIM_IDLE;
    }
    break;

  case ANIM_ANNOYED:
    {
      uint32_t elapsed = now - anim_start_tm;
      float n = sinf((float)(now % 420U) / 420.0f * 6.28318f);
      pet_blink_l = 0.23f; pet_blink_r = 0.23f;
      pet_mouth = 0.0f;
      pet_cheek = 0.0f;
      pet_brow_y = -0.62f + n * 0.05f;
      pet_look_x = lerp(pet_look_x, -0.70f, 0.10f);
      pet_look_y = lerp(pet_look_y, 0.10f, 0.10f);
      if (elapsed > 1500U) pet_state = ANIM_IDLE;
    }
    break;

  // --- Sensor-driven expressions (hold while sensor active) ---

  case ANIM_DIZZY:
    {
      float t = (float)(now % 760U) / 760.0f;
      float w = sinf(t * 6.28318f * 1.4f);
      pet_mouth = 0.62f + w * 0.15f;
      pet_blink_l = (sinf(t * 6.28318f * 2.1f) > 0.08f) ? 0.05f : 0.55f;
      pet_blink_r = (sinf(t * 6.28318f * 2.1f + 1.7f) > 0.08f) ? 0.05f : 0.55f;
      pet_look_x = sinf(t * 6.28318f * 1.5f) * 0.85f;
      pet_look_y = cosf(t * 6.28318f * 1.7f) * 0.62f;
      pet_brow_y = -0.24f + sinf(t * 6.28318f) * 0.45f;
      pet_cheek = 0.0f;
    }
    break;

  case ANIM_PETTED:
    {
      float br = (float)(now % 4200U) / 4200.0f * 6.28318f;
      pet_blink_l = 0.28f + sinf(br * 1.3f) * 0.10f;
      pet_blink_r = 0.28f + sinf(br * 1.5f) * 0.10f;
      pet_mouth = 0.34f + sinf(br * 0.9f) * 0.04f;
      pet_cheek = lerp(pet_cheek, 0.95f, 0.12f);
      pet_brow_y = 0.55f + sinf(br * 0.7f) * 0.16f;
      pet_look_x = sinf(br * 0.7f) * 0.52f;
      pet_look_y = -0.18f + cosf(br * 0.9f) * 0.22f;
    }
    break;

  case ANIM_COLD:
    {
      float shiver = sinf((float)(now % 360U) / 360.0f * 6.28318f * 2.4f);
      pet_blink_l = 0.16f + fabsf(shiver) * 0.15f;
      pet_blink_r = 0.16f + fabsf(shiver) * 0.15f;
      pet_mouth = 0.0f;
      pet_cheek = 0.0f;
      pet_brow_y = -0.56f + shiver * 0.13f;
      pet_look_x = shiver * 0.32f;
      pet_look_y = 0.08f;
    }
    break;

  case ANIM_COMFY:
    {
      float br = (float)(now % 5200U) / 5200.0f * 6.28318f;
      pet_blink_l = 0.16f + sinf(br * 1.2f) * 0.06f;
      pet_blink_r = 0.16f + sinf(br * 1.4f) * 0.06f;
      pet_mouth = 0.23f + sinf(br * 0.8f) * 0.03f;
      pet_cheek = lerp(pet_cheek, 0.62f, 0.06f);
      pet_brow_y = 0.30f + sinf(br * 0.7f) * 0.14f;
      pet_look_x = sinf(br * 0.5f) * 0.35f;
      pet_look_y = cosf(br * 0.7f) * 0.20f;
    }
    break;

  case ANIM_HOT:
    {
      float br = (float)(now % 3300U) / 3300.0f * 6.28318f;
      float pant = sinf((float)(now % 320U) / 320.0f * 6.28318f);
      pet_blink_l = 0.42f + sinf(br * 1.2f) * 0.05f;
      pet_blink_r = 0.42f + sinf(br * 1.4f) * 0.05f;
      pet_mouth = 0.58f + pant * 0.17f;
      pet_cheek = 0.36f;
      pet_brow_y = 0.38f;
      pet_look_x = sinf(br * 0.5f) * 0.30f;
      pet_look_y = 0.32f + cosf(br * 0.6f) * 0.14f;
    }
    break;

  case ANIM_DARK:
    {
      float br = (float)(now % 3600U) / 3600.0f * 6.28318f;
      pet_blink_l = 0.02f + fabsf(sinf(br * 1.4f)) * 0.08f;
      pet_blink_r = 0.02f + fabsf(sinf(br * 1.3f)) * 0.08f;
      pet_mouth = 0.08f;
      pet_cheek = 0.0f;
      pet_brow_y = 0.78f + sinf(br * 0.5f) * 0.11f;
      pet_look_x = sinf(br * 0.9f) * 0.72f;
      pet_look_y = 0.05f + cosf(br * 0.7f) * 0.28f;
    }
    break;

  case ANIM_BRIGHT:
    {
      float br = (float)(now % 4300U) / 4300.0f * 6.28318f;
      pet_blink_l = 0.58f + sinf(br * 1.2f) * 0.07f;
      pet_blink_r = 0.58f + sinf(br * 1.4f) * 0.07f;
      pet_mouth = 0.0f;
      pet_cheek = 0.0f;
      pet_brow_y = -0.52f + sinf(br * 0.5f) * 0.10f;
      pet_look_x = sinf(br * 0.4f) * 0.25f;
      pet_look_y = 0.18f + cosf(br * 0.5f) * 0.10f;
    }
    break;
  }

  if (pet_state != ANIM_HAPPY && pet_state != ANIM_PETTED && pet_state != ANIM_COMFY &&
      pet_state != ANIM_SHY && pet_cheek > 0.0f) {
    pet_cheek = lerp(pet_cheek, 0.0f, 0.05f);
    if (pet_cheek < 0.01f) pet_cheek = 0.0f;
  }
}

// ============ Main loop ============

void pet_launcher_run(void) {
  EMO_Init();
  EHW_Init();

  uint32_t now = HAL_GetTick();
  next_blink_tm = now + 2300U + rnd(3000U);
  next_saccade_tm = now + 1000U;
  mood_timer = now + 5200U + rnd(5000U);
  sensor_timer = 0;
  sensor_expr_start = 0;
  sensor_cooldown_until = 0;
  enter_idx = 0;
  last_enter_pressed = 0;
  enter_tm[0] = enter_tm[1] = enter_tm[2] = 0;

  pet_blink_l = 0.0f; pet_blink_r = 0.0f;
  pet_mouth = 0.0f; pet_cheek = 0.0f;
  pet_look_x = 0.0f; pet_look_y = 0.0f;
  pet_look_tx = 0.0f; pet_look_ty = 0.0f;
  pet_brow_y = 0.0f;
  pet_state = ANIM_IDLE;
  prev_pet_state = ANIM_IDLE;

  LOG_I("PET", "Launcher started - triple-press ENTER to exit");

  uint32_t heartbeat = 0;
  while (1) {
    now = HAL_GetTick();
    if (now - heartbeat > 5000U) {
      heartbeat = now;
      LOG_D("PET", "alive @ %lums, state=%d, expr=%d", (unsigned long)now, pet_state, EHW_GetExpr());
    }

    // --- Input: triple-press ENTER to exit ---
    // Use edge detection. The old logic counted a long hold as multiple presses.
    keyManager.btn_enter.tick();
    uint8_t enter_pressed = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (enter_pressed && !last_enter_pressed) {
      enter_tm[enter_idx % 3] = now;
      enter_idx++;
      if (enter_idx >= 3) {
        uint32_t t0 = enter_tm[(enter_idx - 3) % 3];
        uint32_t t2 = enter_tm[(enter_idx - 1) % 3];
        if (t2 - t0 < TRIPLE_WINDOW) {
          LOG_I("PET", "Triple ENTER - returning to menu");
          EMO_FillScreen(EMO_BLACK);
          LCD_Flush();
          return;
        }
      }
    }
    last_enter_pressed = enter_pressed;

    // --- Update & Draw ---
    boardLCD.updateAutoBrightness();
    update_animation();
    EMO_DrawFace(pet_blink_l, pet_blink_r, pet_mouth,
                 pet_look_x, pet_look_y, pet_cheek, pet_brow_y);
    LCD_Flush();
    HAL_Delay(10);
  }
}
