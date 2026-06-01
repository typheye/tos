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
static CCMRAM uint32_t last_activity_tm = 0;
static CCMRAM uint32_t activity_pulse_until = 0;
static CCMRAM uint32_t state_since_tm = 0;
static CCMRAM int last_logged_state = -1;
static CCMRAM uint8_t last_collision_active = 0;
static CCMRAM uint8_t last_group1_cfg = 0;
static CCMRAM uint8_t last_group2_cfg = 0;
static CCMRAM uint8_t last_group3_cfg = 0;
static CCMRAM uint8_t last_mute_state = 0;
static CCMRAM uint8_t last_sd_state = 0;

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
  ANIM_GLANCE,
  ANIM_GIGGLE,
  ANIM_STRETCH,
  ANIM_NAP,
  ANIM_WAKE,
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
static CCMRAM float pet_touch_mood = 0.0f;
static CCMRAM float pet_touch_mood_target = 0.0f;
static CCMRAM uint32_t pet_touch_mood_since = 0;

// Recovery blending after sensor-driven expressions release.
// Captures the face parameters at release time and smoothly decays them
// toward neutral so the transition doesn't snap to the idle blink cycle.
static CCMRAM uint32_t sensor_recover_start = 0;
static CCMRAM float recover_from_blink_l = 0.0f;
static CCMRAM float recover_from_blink_r = 0.0f;
static CCMRAM float recover_from_mouth   = 0.0f;
static CCMRAM float recover_from_look_x  = 0.0f;
static CCMRAM float recover_from_look_y  = 0.0f;
static CCMRAM float recover_from_cheek   = 0.0f;
static CCMRAM float recover_from_brow_y  = 0.0f;

#define SENSOR_MAX_HOLD       5200U
#define SENSOR_REST_MS        180U
#define SENSOR_POLL_MS        90U
#define SENSOR_MOTION_MIN_MS  1600U
#define TOUCH_STYLE_KEEP_MS   4300U
#define SENSOR_RECOVER_MS     620U

// Triple-press ENTER to exit
static CCMRAM uint32_t enter_tm[3] = {0};
static CCMRAM int enter_idx = 0;
static CCMRAM uint8_t last_enter_pressed = 0;

#define TRIPLE_WINDOW   800U
#define BLINK_DUR_MS    420U
#define WINK_DUR_MS     380U
#define DBLINK_DUR_MS   700U
#define IDLE_NAP_MS     60000U
#define ACTIVITY_PULSE_MS 700U

static uint32_t rnd(uint32_t max) {
  static uint32_t seed = 0xBEEF;
  seed = seed * 1103515245U + 12345U;
  return max ? (seed % max) : 0U;
}

static const char *pet_anim_name(int s) {
  switch (s) {
  case ANIM_IDLE:      return "idle";
  case ANIM_BLINK:     return "blink";
  case ANIM_WINK:      return "wink";
  case ANIM_DBLINK:    return "dblnk";
  case ANIM_HAPPY:     return "happy";
  case ANIM_SURPRISED: return "surprise";
  case ANIM_CURIOUS:   return "curious";
  case ANIM_SLEEPY:    return "sleepy";
  case ANIM_SHY:       return "shy";
  case ANIM_PROUD:     return "proud";
  case ANIM_ANNOYED:   return "annoyed";
  case ANIM_GLANCE:    return "glance";
  case ANIM_GIGGLE:    return "giggle";
  case ANIM_STRETCH:   return "stretch";
  case ANIM_NAP:       return "nap";
  case ANIM_WAKE:      return "wake";
  case ANIM_DIZZY:     return "dizzy";
  case ANIM_PETTED:    return "petted";
  case ANIM_COLD:      return "cold";
  case ANIM_COMFY:     return "comfy";
  case ANIM_HOT:       return "hot";
  case ANIM_DARK:      return "dark";
  case ANIM_BRIGHT:    return "bright";
  default:             return "?";
  }
}

static void mark_activity(uint32_t now, const char *reason, bool log_it) {
  last_activity_tm = now;
  activity_pulse_until = now + ACTIVITY_PULSE_MS;
  if (log_it) {
    LOG_D("PET", "activity: %s @ %lums", reason, (unsigned long)now);
  }
}

static void trace_state(uint32_t now) {
  if (pet_state == last_logged_state) return;
  LOG_D("PET", "state: %s -> %s, idle=%lums expr=%d",
        last_logged_state < 0 ? "boot" : pet_anim_name(last_logged_state),
        pet_anim_name(pet_state),
        (unsigned long)(now - last_activity_tm),
        EHW_GetExpr());
  last_logged_state = pet_state;
  state_since_tm = now;
}

static void choose_touch_style(uint32_t now) {
  if (now - pet_touch_mood_since < TOUCH_STYLE_KEEP_MS) return;
  pet_touch_mood_target = (rnd(100U) < 32U) ? 1.0f : 0.0f; // 0=blush smile, 1=pout
  pet_touch_mood_since = now;
  LOG_D("PET", "touch style=%s", pet_touch_mood_target > 0.5f ? "pout" : "smile");
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
  // Capture current face parameters for smooth recovery blend.
  // This prevents the jarring jump from dizzy/petted straight into
  // the idle blink cycle — the face eases back to neutral instead.
  recover_from_blink_l = pet_blink_l;
  recover_from_blink_r = pet_blink_r;
  recover_from_mouth   = pet_mouth;
  recover_from_look_x  = pet_look_x;
  recover_from_look_y  = pet_look_y;
  recover_from_cheek   = pet_cheek;
  recover_from_brow_y  = pet_brow_y;
  sensor_recover_start = now;
  pet_state = ANIM_IDLE;
  prev_pet_state = ANIM_IDLE;
  // Keep motion/gravity highly responsive.  The old multi-second rest period
  // made random expressions look like they were stealing sensor events.
  sensor_cooldown_until = now + (was_motion ? SENSOR_REST_MS : 900U);
  // Delay the first blink so recovery can finish before idle animation kicks in.
  next_blink_tm = now + SENSOR_RECOVER_MS + 600U + rnd(1200U);
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
      if (is_motion_anim(pet_state) && now - sensor_expr_start < SENSOR_MOTION_MIN_MS) return;
      release_sensor_expression(now);
    }
    return;
  }

  // Motion/gravity events are high priority and can preempt any random mood.
  // Slow environment moods still respect a short cooldown to avoid flicker.
  if (now < sensor_cooldown_until && !is_motion_anim(anim)) return;

  // Idle sleep is based on real user activity, not on ambient light/temp moods.
  // Let the pet keep napping even if the room is bright or cold.
  if (now - last_activity_tm >= IDLE_NAP_MS && !is_motion_anim(anim)) return;

  if (pet_state != anim) {
    if (is_motion_anim(pet_state) && !is_motion_anim(anim)) {
      if (now - sensor_expr_start < SENSOR_MOTION_MIN_MS) return;
      release_sensor_expression(now);
      return;
    }
    if (pet_state == ANIM_DIZZY && anim == ANIM_PETTED &&
        now - sensor_expr_start < SENSOR_MOTION_MIN_MS) {
      return;
    }
    // After DIZZY releases, block PETTED for the recovery window so the
    // same handling that shook the device doesn't immediately re-trigger.
    if (anim == ANIM_PETTED &&
        now - sensor_recover_start < SENSOR_RECOVER_MS + 400U) {
      return;
    }
    if (!is_sensor_anim(pet_state)) prev_pet_state = pet_state;
    bool wake_from_sleep = (pet_state == ANIM_NAP || pet_state == ANIM_WAKE);
    if (anim == ANIM_DIZZY || wake_from_sleep) mark_activity(now, "imu", false);
    if (anim == ANIM_PETTED) choose_touch_style(now);
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

  // Smooth recovery blend after sensor-driven expressions (DIZZY, PETTED, etc.)
  // release.  The captured face parameters decay toward neutral over ~620ms so
  // the transition doesn't snap jarringly into the idle blink cycle.
  if (now - sensor_recover_start < SENSOR_RECOVER_MS) {
    float raw_t = (float)(now - sensor_recover_start) / (float)SENSOR_RECOVER_MS;
    float t = ease_inout(raw_t);
    pet_blink_l = lerp(recover_from_blink_l, 0.0f, t);
    pet_blink_r = lerp(recover_from_blink_r, 0.0f, t);
    pet_mouth   = lerp(recover_from_mouth,   0.0f, t);
    pet_look_x  = lerp(recover_from_look_x,  pet_look_tx, t);
    pet_look_y  = lerp(recover_from_look_y,  0.0f, t);
    pet_cheek   = lerp(recover_from_cheek,   0.0f, t);
    pet_brow_y  = lerp(recover_from_brow_y,  0.0f, t);
  }

  switch (pet_state) {

  case ANIM_IDLE:
    if (pet_cheek > 0.0f) pet_cheek = lerp(pet_cheek, 0.0f, 0.06f);
    if (now - last_activity_tm >= IDLE_NAP_MS) {
      pet_state = ANIM_NAP;
      anim_start_tm = now;
      blink_phase = 0;
      LOG_I("PET", "Idle %lums, entering nap", (unsigned long)(now - last_activity_tm));
      break;
    }
    // During sensor recovery the face is still blending toward neutral;
    // don't interrupt it with a blink or random mood.
    if (now - sensor_recover_start < SENSOR_RECOVER_MS) break;
    if (now >= next_blink_tm) {
      int r = rnd(12U);
      if (r < 2)      { pet_state = ANIM_WINK; anim_start_tm = now; blink_phase = 0; }
      else if (r < 4) { pet_state = ANIM_DBLINK; anim_start_tm = now; blink_phase = 0; }
      else            { pet_state = ANIM_BLINK; anim_start_tm = now; blink_phase = 0; }
    }

    // Do not start a random mood while the sensor engine has an active event
    // or while the face is still in post-sensor recovery.
    if (EHW_GetExpr() == EHW_EXPR_NONE &&
        now - sensor_recover_start >= SENSOR_RECOVER_MS &&
        now - mood_timer > 6200U + rnd(7600U)) {
      mood_timer = now;
      int r = rnd(24U);
      if (r < 2)       pet_state = ANIM_SURPRISED;
      else if (r < 5)  pet_state = ANIM_HAPPY;
      else if (r < 8)  pet_state = ANIM_CURIOUS;
      else if (r < 10) pet_state = ANIM_SLEEPY;
      else if (r < 12) pet_state = ANIM_SHY;
      else if (r < 14) pet_state = ANIM_PROUD;
      else if (r < 16) pet_state = ANIM_ANNOYED;
      else if (r < 19) pet_state = ANIM_GLANCE;
      else if (r < 22) pet_state = ANIM_GIGGLE;
      else             pet_state = ANIM_STRETCH;
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

  case ANIM_GLANCE:
    {
      uint32_t elapsed = now - anim_start_tm;
      int dir = ((anim_start_tm >> 3) & 1U) ? 1 : -1;
      float t = elapsed < 520U ? (float)elapsed / 520.0f : 1.0f;
      float settle = elapsed > 1600U ? (float)(elapsed - 1600U) / 620.0f : 0.0f;
      if (settle > 1.0f) settle = 1.0f;
      float target_x = (float)dir * (0.82f - settle * 0.82f);
      pet_look_x = lerp(pet_look_x, target_x, 0.12f);
      pet_look_y = lerp(pet_look_y, -0.10f + sinf((float)(now % 700U) / 700.0f * 6.28318f) * 0.06f, 0.08f);
      pet_brow_y = lerp(pet_brow_y, 0.28f * (1.0f - settle), 0.08f);
      pet_mouth = lerp(pet_mouth, 0.10f + t * 0.05f, 0.08f);
      if (elapsed > 2300U) pet_state = ANIM_IDLE;
    }
    break;

  case ANIM_GIGGLE:
    {
      uint32_t elapsed = now - anim_start_tm;
      float wave = sinf((float)(now % 360U) / 360.0f * 6.28318f);
      pet_cheek = lerp(pet_cheek, 0.96f, 0.15f);
      pet_mouth = 0.36f + fabsf(wave) * 0.10f;
      pet_brow_y = 0.38f + wave * 0.08f;
      pet_blink_l = 0.34f + (wave > 0.0f ? 0.12f : 0.0f);
      pet_blink_r = 0.28f + (wave < 0.0f ? 0.10f : 0.0f);
      pet_look_x = lerp(pet_look_x, wave * 0.20f, 0.10f);
      pet_look_y = lerp(pet_look_y, -0.12f, 0.09f);
      if (elapsed > 1900U) pet_state = ANIM_IDLE;
    }
    break;

  case ANIM_STRETCH:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (elapsed < 700U) {
        float t = ease_inout((float)elapsed / 700.0f);
        pet_blink_l = t * 0.82f;
        pet_blink_r = t * 0.82f;
        pet_mouth = 0.15f + t * 0.55f;
        pet_brow_y = 0.35f + t * 0.36f;
        pet_look_y = lerp(pet_look_y, 0.28f, 0.10f);
      } else if (elapsed < 1550U) {
        float wobble = sinf((float)(now % 520U) / 520.0f * 6.28318f);
        pet_blink_l = 0.82f;
        pet_blink_r = 0.82f;
        pet_mouth = 0.62f + wobble * 0.07f;
        pet_brow_y = 0.58f + wobble * 0.08f;
        pet_look_x = wobble * 0.18f;
        pet_look_y = 0.32f;
      } else if (elapsed < 2300U) {
        float t = ease_inout((float)(elapsed - 1550U) / 750.0f);
        pet_blink_l = 0.82f * (1.0f - t);
        pet_blink_r = 0.82f * (1.0f - t);
        pet_mouth = 0.62f * (1.0f - t);
        pet_brow_y = 0.58f * (1.0f - t);
        pet_look_y = 0.32f * (1.0f - t);
      } else {
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_NAP:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (now < activity_pulse_until) {
        pet_state = ANIM_WAKE;
        anim_start_tm = now;
        blink_phase = 0;
        LOG_I("PET", "Wake from nap after %lums", (unsigned long)elapsed);
        break;
      }
      float br = (float)(now % 3600U) / 3600.0f * 6.28318f;
      float deep = elapsed < 1800U ? ease_inout((float)elapsed / 1800.0f) : 1.0f;
      pet_blink_l = lerp(pet_blink_l, 0.96f, 0.10f);
      pet_blink_r = lerp(pet_blink_r, 0.96f, 0.10f);
      pet_mouth = 0.12f + (sinf(br) + 1.0f) * 0.045f * deep;
      pet_cheek = lerp(pet_cheek, 0.18f, 0.04f);
      pet_brow_y = lerp(pet_brow_y, 0.16f + sinf(br * 0.5f) * 0.08f, 0.05f);
      pet_look_x = lerp(pet_look_x, sinf(br * 0.35f) * 0.06f, 0.03f);
      pet_look_y = lerp(pet_look_y, 0.86f, 0.08f);
    }
    break;

  case ANIM_WAKE:
    {
      uint32_t elapsed = now - anim_start_tm;
      if (elapsed < 520U) {
        float t = ease_inout((float)elapsed / 520.0f);
        pet_blink_l = 0.96f - t * 0.42f;
        pet_blink_r = 0.96f - t * 0.42f;
        pet_mouth = 0.22f + t * 0.38f;
        pet_brow_y = 0.15f + t * 0.26f;
        pet_look_y = 0.80f - t * 0.42f;
      } else if (elapsed < 1050U) {
        float t = ease_inout((float)(elapsed - 520U) / 530.0f);
        pet_blink_l = 0.54f * (1.0f - t);
        pet_blink_r = 0.54f * (1.0f - t);
        pet_mouth = 0.60f * (1.0f - t);
        pet_brow_y = 0.40f * (1.0f - t);
        pet_look_y = 0.38f * (1.0f - t);
      } else {
        pet_blink_l = 0.0f;
        pet_blink_r = 0.0f;
        pet_mouth = 0.0f;
        pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
        next_blink_tm = now + 1200U + rnd(1800U);
        mood_timer = now + 4200U + rnd(4200U);
      }
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
      uint32_t elapsed = now - sensor_expr_start;
      if (pet_touch_mood_target > 0.5f && elapsed > 2600U) {
        pet_touch_mood_target = 0.0f;
      }
      pet_touch_mood = lerp(pet_touch_mood, pet_touch_mood_target, 0.035f);
      float pout = pet_touch_mood;
      float smile = 1.0f - pout;
      float wave = sinf(br * 0.9f);
      float blink_target = smile * (0.28f + sinf(br * 1.3f) * 0.10f) +
                           pout * (0.10f + fabsf(wave) * 0.06f);
      float mouth_target = smile * (0.34f + wave * 0.04f) +
                           pout * (0.54f + sinf(br * 1.6f) * 0.025f);
      float brow_target = smile * (0.55f + sinf(br * 0.7f) * 0.16f) +
                          pout * (0.08f + sinf(br * 0.6f) * 0.05f);
      float look_x_target = smile * (sinf(br * 0.7f) * 0.52f) +
                            pout * (sinf(br * 0.55f) * 0.18f);
      float look_y_target = smile * (-0.18f + cosf(br * 0.9f) * 0.22f) +
                            pout * (0.02f + cosf(br * 0.8f) * 0.05f);
      pet_blink_l = lerp(pet_blink_l, blink_target, 0.10f);
      pet_blink_r = lerp(pet_blink_r, blink_target + sinf(br * 1.5f) * 0.025f, 0.10f);
      pet_mouth = lerp(pet_mouth, mouth_target, 0.08f);
      pet_cheek = lerp(pet_cheek, 0.95f, 0.12f);
      pet_brow_y = lerp(pet_brow_y, brow_target, 0.08f);
      pet_look_x = lerp(pet_look_x, look_x_target, 0.08f);
      pet_look_y = lerp(pet_look_y, look_y_target, 0.08f);
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
      pet_state != ANIM_SHY && pet_state != ANIM_GIGGLE && pet_state != ANIM_NAP &&
      pet_cheek > 0.0f) {
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
  last_activity_tm = now;
  activity_pulse_until = 0;
  state_since_tm = now;
  last_logged_state = -1;
  sensor_timer = 0;
  sensor_expr_start = 0;
  sensor_cooldown_until = 0;
  sensor_recover_start = 0;
  enter_idx = 0;
  last_enter_pressed = 0;
  enter_tm[0] = enter_tm[1] = enter_tm[2] = 0;
  last_collision_active = keyManager.isAnyCollision() ? 1U : 0U;
  last_group1_cfg = keyManager.getGroup1Config();
  last_group2_cfg = keyManager.getGroup2Config();
  last_group3_cfg = keyManager.getGroup3Config();
  last_mute_state = keyManager.isMuted() ? 1U : 0U;
  last_sd_state = keyManager.isSdCardInserted() ? 1U : 0U;

  pet_blink_l = 0.0f; pet_blink_r = 0.0f;
  pet_mouth = 0.0f; pet_cheek = 0.0f;
  pet_look_x = 0.0f; pet_look_y = 0.0f;
  pet_look_tx = 0.0f; pet_look_ty = 0.0f;
  pet_brow_y = 0.0f;
  pet_state = ANIM_IDLE;
  prev_pet_state = ANIM_IDLE;
  pet_touch_mood = 0.0f;
  pet_touch_mood_target = 0.0f;
  pet_touch_mood_since = 0;

  LOG_I("PET", "Launcher started - triple-press ENTER to exit");

  uint32_t heartbeat = 0;
  while (1) {
    now = HAL_GetTick();
    if (now - heartbeat > 5000U) {
      heartbeat = now;
      LOG_D("PET", "alive @ %lums, state=%s, expr=%d, idle=%lums, state_age=%lums",
            (unsigned long)now, pet_anim_name(pet_state), EHW_GetExpr(),
            (unsigned long)(now - last_activity_tm),
            (unsigned long)(now - state_since_tm));
    }

    // --- Input: triple-press ENTER to exit ---
    // Use edge detection. The old logic counted a long hold as multiple presses.
    keyManager.tick();
    uint8_t enter_pressed = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (enter_pressed && !last_enter_pressed) {
      mark_activity(now, "enter", true);
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

    uint8_t collision_active = keyManager.isAnyCollision() ? 1U : 0U;
    if (collision_active && !last_collision_active) {
      mark_activity(now, "collision", true);
    }
    last_collision_active = collision_active;

    uint8_t group1_cfg = keyManager.getGroup1Config();
    uint8_t group2_cfg = keyManager.getGroup2Config();
    uint8_t group3_cfg = keyManager.getGroup3Config();
    uint8_t mute_state = keyManager.isMuted() ? 1U : 0U;
    uint8_t sd_state = keyManager.isSdCardInserted() ? 1U : 0U;
    if (group1_cfg != last_group1_cfg || group2_cfg != last_group2_cfg ||
        group3_cfg != last_group3_cfg || mute_state != last_mute_state ||
        sd_state != last_sd_state) {
      mark_activity(now, "switch", true);
      LOG_D("PET", "switch cfg g1=%u g2=%u g3=%u mute=%u sd=%u",
            group1_cfg, group2_cfg, group3_cfg, mute_state, sd_state);
      last_group1_cfg = group1_cfg;
      last_group2_cfg = group2_cfg;
      last_group3_cfg = group3_cfg;
      last_mute_state = mute_state;
      last_sd_state = sd_state;
    }

    // --- Update & Draw ---
    boardLCD.updateAutoBrightness();
    update_animation();
    trace_state(now);
    EMO_DrawFace(pet_blink_l, pet_blink_r, pet_mouth,
                 pet_look_x, pet_look_y, pet_cheek, pet_brow_y);
    LCD_Flush();
    HAL_Delay(10);
  }
}
