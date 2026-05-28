#include "include/launcher.hpp"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libemo.h"
#include <cmath>
#include <cstdio>

extern KeyManager keyManager;
extern LCD boardLCD;

// ============ Animation state ============
static float pet_blink_l  = 0.0f;
static float pet_blink_r  = 0.0f;
static float pet_mouth    = 0.0f;
static float pet_look_x   = 0.0f;
static float pet_look_y   = 0.0f;
static float pet_cheek    = 0.0f;
static float pet_brow_y   = 0.0f;
static float pet_bob      = 0.0f;

enum PetAnim {
  ANIM_IDLE = 0,
  ANIM_BLINK,
  ANIM_WINK,
  ANIM_DBLINK,    // double blink (rapid)
  ANIM_HAPPY,
  ANIM_SURPRISED,
  ANIM_CURIOUS,   // raised brows, slight smile
};
static int  pet_state    = ANIM_IDLE;
static int  blink_phase  = 0;
static int  anim_frame   = 0;

static uint32_t next_blink_tm  = 0;
static uint32_t anim_start_tm  = 0;
static uint32_t mood_timer     = 0;

// Long-press
static uint32_t enter_held_tm  = 0;
static bool     enter_was_down = false;

#define LONG_PRESS_MS   700
#define BLINK_DUR_MS    250
#define WINK_DUR_MS     240
#define DBLINK_DUR_MS   500

static uint32_t rnd(uint32_t max) {
  static uint32_t seed = 0xBEEF;
  seed = seed * 1103515245 + 12345;
  return seed % max;
}

// ============ Easing ============

static float ease_in(float t)  { return t * t; }
static float ease_out(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
static float ease_inout(float t) {
  return t < 0.5f ? 2*t*t : 1 - (-2*t+2)*(-2*t+2)/2;
}

// ============ Per-frame update ============

static void update_idle_motion(void) {
  uint32_t now = HAL_GetTick();
  // Bob always runs (gentle breathing)
  float bt = (float)(now % 3200) / 3200.0f;
  pet_bob = sinf(bt * 2.0f * 3.14159f) * 1.5f;

  // Eye wander + brow only in idle (other states control their own)
  if (pet_state != ANIM_IDLE) return;

  float lt = (float)(now % 4500) / 4500.0f;
  float phase = lt * 2.0f * 3.14159f;
  pet_look_x = sinf(phase) * 0.55f;
  pet_look_y = cosf(phase * 1.3f) * 0.25f;
  pet_brow_y = sinf(phase * 0.7f) * 0.2f;
}

static void update_animation(void) {
  uint32_t now = HAL_GetTick();

  update_idle_motion();

  switch (pet_state) {

  case ANIM_IDLE:
    pet_mouth   = 0.0f;
    pet_blink_l = 0.0f;
    pet_blink_r = 0.0f;
    pet_cheek   = 0.0f;

    if (now >= next_blink_tm) {
      int r = rnd(10);
      if (r < 2) {
        pet_state = ANIM_WINK;
        anim_start_tm = now;
        blink_phase = 0;
      } else if (r < 4) {
        pet_state = ANIM_DBLINK;
        anim_start_tm = now;
        blink_phase = 0;
      } else {
        pet_state = ANIM_BLINK;
        anim_start_tm = now;
        blink_phase = 0;
      }
    }

    // Periodic mood change
    if (now - mood_timer > 8000 + rnd(7000)) {
      mood_timer = now;
      int r = rnd(10);
      if (r < 2) {
        pet_state = ANIM_SURPRISED;
        anim_start_tm = now;
      } else if (r < 5) {
        pet_state = ANIM_HAPPY;
        anim_start_tm = now;
      } else {
        pet_state = ANIM_CURIOUS;
        anim_start_tm = now;
      }
    }
    break;

  case ANIM_BLINK:
    {
      int elapsed = (int)(now - anim_start_tm);
      int half = BLINK_DUR_MS / 2;

      if (blink_phase == 0) {
        // Closing: ease-in (accelerates like real eyelid)
        if (elapsed < half) {
          float t = (float)elapsed / half;
          pet_blink_l = ease_in(t);
          pet_blink_r = ease_in(t);
        } else {
          pet_blink_l = 1.0f; pet_blink_r = 1.0f;
          blink_phase = 1;
        }
      } else {
        // Opening: ease-out (decelerates naturally)
        int t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / half;
          pet_blink_l = 1.0f - ease_out(t);
          pet_blink_r = 1.0f - ease_out(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE;
          next_blink_tm = now + 2500 + rnd(3500);
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
          pet_blink_l = ease_in(t);
          pet_blink_r = 0.0f;
        } else {
          pet_blink_l = 1.0f;
          blink_phase = 1;
        }
      } else {
        int t2 = elapsed - half;
        if (t2 < half) {
          float t = (float)t2 / half;
          pet_blink_l = 1.0f - ease_out(t);
        } else {
          pet_blink_l = 0.0f; pet_blink_r = 0.0f;
          pet_state = ANIM_IDLE;
          next_blink_tm = now + 2500 + rnd(3000);
        }
      }
    }
    break;

  case ANIM_DBLINK:
    {
      int elapsed = (int)(now - anim_start_tm);
      int seg = DBLINK_DUR_MS / 4;

      // close-1 (ease-in)
      if (elapsed < seg) {
        float t = (float)elapsed / seg;
        pet_blink_l = ease_in(t); pet_blink_r = ease_in(t);
      // open-1 (ease-out)
      } else if (elapsed < seg * 2) {
        float t = (float)(elapsed - seg) / seg;
        pet_blink_l = 1.0f - ease_out(t); pet_blink_r = 1.0f - ease_out(t);
      // close-2 (ease-in)
      } else if (elapsed < seg * 3) {
        float t = (float)(elapsed - seg * 2) / seg;
        pet_blink_l = ease_in(t); pet_blink_r = ease_in(t);
      // open-2 (ease-out)
      } else if (elapsed < seg * 4) {
        float t = (float)(elapsed - seg * 3) / seg;
        pet_blink_l = 1.0f - ease_out(t); pet_blink_r = 1.0f - ease_out(t);
      } else {
        pet_blink_l = 0.0f; pet_blink_r = 0.0f;
        pet_state = ANIM_IDLE;
        next_blink_tm = now + 2500 + rnd(3000);
      }
    }
    break;

  case ANIM_HAPPY:
    {
      int elapsed = (int)(now - anim_start_tm);
      // Fade in cheek + widen mouth
      if (elapsed < 300) {
        float t = (float)elapsed / 300.0f;
        pet_mouth = t * 0.35f;
        pet_cheek = t;
        pet_brow_y = t * 0.4f;
      } else if (elapsed < 1800) {
        pet_mouth = 0.35f;
        pet_cheek = 1.0f;
        pet_brow_y = 0.4f;
      } else if (elapsed < 2200) {
        float t = (float)(elapsed - 1800) / 400.0f;
        pet_mouth = 0.35f * (1.0f - t);
        pet_cheek = 1.0f - t;
        pet_brow_y = 0.4f * (1.0f - t);
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
        pet_mouth = 0.5f + t * 0.5f;
        pet_brow_y = t * 1.0f;
      } else if (elapsed < 1200) {
        pet_mouth = 1.0f;
        pet_brow_y = 1.0f;
      } else if (elapsed < 1500) {
        float t = (float)(elapsed - 1200) / 300.0f;
        pet_mouth = 1.0f * (1.0f - t);
        pet_brow_y = 1.0f * (1.0f - t);
      } else {
        pet_mouth = 0.0f; pet_brow_y = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;

  case ANIM_CURIOUS:
    {
      int elapsed = (int)(now - anim_start_tm);
      // Slightly raised brow, subtle smile, slight head tilt look
      if (elapsed < 400) {
        float t = (float)elapsed / 400.0f;
        pet_brow_y = t * 0.55f;
        pet_mouth = t * 0.18f;
        pet_look_x = t * 0.7f;
      } else if (elapsed < 2000) {
        pet_brow_y = 0.55f;
        pet_mouth = 0.18f;
        pet_look_x = 0.7f;
      } else if (elapsed < 2600) {
        float t = (float)(elapsed - 2000) / 600.0f;
        pet_brow_y = 0.55f * (1.0f - t);
        pet_mouth = 0.18f * (1.0f - t);
        pet_look_x = 0.7f * (1.0f - t);
      } else {
        pet_brow_y = 0.0f; pet_mouth = 0.0f; pet_look_x = 0.0f;
        pet_state = ANIM_IDLE;
      }
    }
    break;
  }

  // Smooth cheek decay (for quick transitions)
  if (pet_state != ANIM_HAPPY && pet_cheek > 0.0f) {
    pet_cheek -= 0.02f;
    if (pet_cheek < 0.0f) pet_cheek = 0.0f;
  }
}

// ============ Main loop ============

void pet_launcher_run(void) {
  EMO_Init();

  next_blink_tm = HAL_GetTick() + 2500 + rnd(3000);
  mood_timer    = HAL_GetTick() + 8000 + rnd(5000);
  enter_was_down = false;
  enter_held_tm  = 0;

  pet_blink_l = 0.0f; pet_blink_r = 0.0f;
  pet_mouth   = 0.0f; pet_cheek   = 0.0f;
  pet_look_x  = 0.0f; pet_look_y  = 0.0f;
  pet_brow_y  = 0.0f; pet_bob     = 0.0f;
  pet_state   = ANIM_IDLE;

  printf("[Pet] Launcher started — hold ENTER %dms to exit\r\n", LONG_PRESS_MS);

  while (1) {
    uint32_t now = HAL_GetTick();

    // --- Input ---
    keyManager.btn_enter.tick();
    bool pressed = keyManager.btn_enter.isPressed();

    if (pressed && !enter_was_down) {
      enter_held_tm = now;
    } else if (pressed && enter_was_down) {
      if (now - enter_held_tm >= LONG_PRESS_MS) {
        printf("[Pet] Long press — returning to menu\r\n");
        // Blink-out exit animation
        for (int f = 0; f < 15; f++) {
          float b = (float)f / 14.0f;
          EMO_DrawFace(b, b, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
          LCD_Flush();
          HAL_Delay(20);
        }
        boardLCD.fillScreen(LCD_COLOR_BLACK);
        return;
      }
    }
    enter_was_down = pressed;

    // --- Update & Draw ---
    update_animation();
    EMO_DrawFace(pet_blink_l, pet_blink_r, pet_mouth,
                 pet_look_x, pet_look_y, pet_cheek, pet_brow_y);
    LCD_Flush();
    HAL_Delay(16);
  }
}
