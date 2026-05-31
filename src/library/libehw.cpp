#include "include/libehw.h"
#include "hardware/include/jy901s.hpp"
#include "hardware/include/bmp180.hpp"
#include "hardware/include/tcs3472.hpp"
#include <math.h>
#include <stdio.h>
#include "syslog.h"

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

static CCMRAM EHW_Expr_t last_expr     = EHW_EXPR_NONE;
static CCMRAM EHW_Expr_t stable_expr   = EHW_EXPR_NONE;
static CCMRAM EHW_Expr_t pending_expr  = EHW_EXPR_NONE;
static CCMRAM uint32_t   last_poll     = 0;
static CCMRAM uint32_t   expr_since    = 0;
static CCMRAM uint32_t   cooldown_until = 0;
static CCMRAM uint32_t   next_comfy_due = 0;
static CCMRAM int        pending_cnt   = 0;
static CCMRAM int        none_cnt      = 0;

#define POLL_MS          90U
#define DEBOUNCE_ON      2
#define DEBOUNCE_OFF     3
#define MIN_HOLD_MS      1200U
#define MOTION_HOLD_MS   120U
#define COOLDOWN_MS      1200U
#define MOTION_COOLDOWN_MS 120U
#define SENSOR_TO_MS     80U
#define COMFY_PERIOD_MS  42000U
#define COMFY_HOLD_MS    2600U

// ============ JY901S motion detection ============
// The old logic looked only at |acc| delta. That misses pure tilt under gravity
// and can also fire falsely when gravity projection changes slowly.  The new
// logic separates low-pass gravity from high-pass linear acceleration, then
// combines it with gyro and tilt-rate.  This makes petting/tilt smoother while
// keeping violent shake as DIZZY.

static CCMRAM float grav_x = 0.0f;
static CCMRAM float grav_y = 0.0f;
static CCMRAM float grav_z = 1.0f;
static CCMRAM float smooth_motion = 0.0f;
static CCMRAM float smooth_gyro = 0.0f;
static CCMRAM float smooth_tilt = 0.0f;
static CCMRAM float smooth_tilt_rate = 0.0f;
static CCMRAM float last_pitch = 0.0f;
static CCMRAM float last_roll = 0.0f;
static CCMRAM bool  imu_valid = false;
static CCMRAM bool  jy_warned = false;
static CCMRAM bool  bmp_warned = false;
static CCMRAM bool  tcs_warned = false;

static float angle_diff(float a, float b) {
  float d = a - b;
  while (d > 180.0f) d -= 360.0f;
  while (d < -180.0f) d += 360.0f;
  return d;
}

static bool is_motion_expr(EHW_Expr_t e) {
  return e == EHW_EXPR_DIZZY || e == EHW_EXPR_PETTED;
}

static EHW_Expr_t check_jy901s(void) {
  if (!boardJY901S.isInitialized()) {
    if (!jy_warned) {
      jy_warned = true;
      LOG_W("EHW", "JY901S not init");
    }
    return EHW_EXPR_NONE;
  }

  uint32_t t0 = HAL_GetTick();
  JY901S_Data_t d = boardJY901S.readData();
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "JY901S read took %lums", (unsigned long)dt);

  float acc_mag = sqrtf(d.acc_x * d.acc_x + d.acc_y * d.acc_y + d.acc_z * d.acc_z);
  if (acc_mag < 0.05f || acc_mag > 8.0f) {
    // Bad read / disconnected bus burst. Decay instead of creating a fake mood.
    smooth_motion *= 0.55f;
    smooth_gyro *= 0.65f;
    smooth_tilt *= 0.88f;
    smooth_tilt_rate *= 0.50f;
    return EHW_EXPR_NONE;
  }

  // Normalize the acceleration vector before using it as gravity.  JY901S can
  // be configured for different acceleration ranges; normalized tilt is stable
  // even if the raw unit is g or m/s^2.
  float nx = d.acc_x / acc_mag;
  float ny = d.acc_y / acc_mag;
  float nz = d.acc_z / acc_mag;

  float roll;
  float pitch;
  if (!imu_valid) {
    grav_x = nx;
    grav_y = ny;
    grav_z = nz;
    roll = atan2f(grav_y, grav_z) * 57.29578f;
    pitch = atan2f(-grav_x, sqrtf(grav_y * grav_y + grav_z * grav_z)) * 57.29578f;
    last_roll = roll;
    last_pitch = pitch;
    imu_valid = true;
  } else {
    // Low-pass gravity.  A faster alpha fixes the previous bug where static
    // board tilt was treated as "nothing" after the first sample.
    grav_x = grav_x * 0.76f + nx * 0.24f;
    grav_y = grav_y * 0.76f + ny * 0.24f;
    grav_z = grav_z * 0.76f + nz * 0.24f;
    roll = atan2f(grav_y, grav_z) * 57.29578f;
    pitch = atan2f(-grav_x, sqrtf(grav_y * grav_y + grav_z * grav_z)) * 57.29578f;
  }

  float lin_x = nx - grav_x;
  float lin_y = ny - grav_y;
  float lin_z = nz - grav_z;
  float lin_mag = sqrtf(lin_x * lin_x + lin_y * lin_y + lin_z * lin_z);
  float gyro_mag = sqrtf(d.gyro_x * d.gyro_x + d.gyro_y * d.gyro_y + d.gyro_z * d.gyro_z);

  float tilt_rate = fabsf(angle_diff(roll, last_roll)) + fabsf(angle_diff(pitch, last_pitch));
  last_roll = roll;
  last_pitch = pitch;

  // Absolute tilt is the key part: holding the board tilted should still be
  // sensed.  The previous version mostly used tilt_delta, so the expression
  // disappeared as soon as the board stopped moving.
  float tilt_abs = fmaxf(fabsf(roll), fabsf(pitch));

  smooth_motion = smooth_motion * 0.62f + lin_mag * 0.38f;
  smooth_gyro = smooth_gyro * 0.58f + gyro_mag * 0.42f;
  smooth_tilt = smooth_tilt * 0.70f + tilt_abs * 0.30f;
  smooth_tilt_rate = smooth_tilt_rate * 0.50f + tilt_rate * 0.50f;

  // Hysteresis while an expression is active.  This avoids flicker but does not
  // block real gravity/IMU events behind the random emotion loop.
  if (stable_expr == EHW_EXPR_DIZZY) {
    if (smooth_motion > 0.34f || smooth_gyro > 68.0f || smooth_tilt_rate > 10.0f || smooth_tilt > 64.0f) {
      return EHW_EXPR_DIZZY;
    }
  }
  if (stable_expr == EHW_EXPR_PETTED) {
    if (smooth_motion > 0.035f || smooth_gyro > 8.5f || smooth_tilt_rate > 1.8f || smooth_tilt > 13.5f) {
      return EHW_EXPR_PETTED;
    }
  }

  // Violent shake / flip: dizzy.
  if (smooth_motion > 0.58f || smooth_gyro > 150.0f || smooth_tilt_rate > 22.0f || smooth_tilt > 78.0f) {
    return EHW_EXPR_DIZZY;
  }

  // Gentle tilt / touch / hand movement: petted.  Absolute tilt makes the
  // "gravity sensing" feel alive even if the module is held still.
  if (smooth_motion > 0.055f || smooth_gyro > 12.0f || smooth_tilt_rate > 2.4f || smooth_tilt > 18.0f) {
    return EHW_EXPR_PETTED;
  }

  return EHW_EXPR_NONE;
}

// ============ BMP180 temperature ============

static CCMRAM float smooth_temp = 22.0f;
static CCMRAM bool  temp_valid = false;

static EHW_Expr_t check_bmp180(uint32_t now) {
  if (!boardBMP180.isInitialized()) {
    if (!bmp_warned) {
      bmp_warned = true;
      LOG_W("EHW", "BMP180 not init");
    }
    return EHW_EXPR_NONE;
  }

  uint32_t t0 = HAL_GetTick();
  float temp = boardBMP180.readTemperature();
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "BMP180 read took %lums", (unsigned long)dt);

  if (temp < -20.0f || temp > 85.0f) return EHW_EXPR_NONE;
  if (!temp_valid) {
    smooth_temp = temp;
    temp_valid = true;
  } else {
    smooth_temp = smooth_temp * 0.92f + temp * 0.08f;
  }
  float t = smooth_temp;

  if (stable_expr == EHW_EXPR_COLD) {
    if (t > 17.5f) return EHW_EXPR_NONE;
    return EHW_EXPR_COLD;
  }
  if (stable_expr == EHW_EXPR_HOT) {
    if (t < 32.5f) return EHW_EXPR_NONE;
    return EHW_EXPR_HOT;
  }
  if (stable_expr == EHW_EXPR_COMFY) {
    return (now - expr_since < COMFY_HOLD_MS) ? EHW_EXPR_COMFY : EHW_EXPR_NONE;
  }

  if (t < 13.0f) return EHW_EXPR_COLD;
  if (t > 36.0f) return EHW_EXPR_HOT;

  // COMFY is a short mood pulse, not a permanent state. Otherwise the pet
  // never returns to its spontaneous expressions in normal room temperature.
  if (t >= 21.0f && t <= 29.5f && now >= next_comfy_due) return EHW_EXPR_COMFY;
  return EHW_EXPR_NONE;
}

// ============ TCS3472 ambient light ============

static CCMRAM float smooth_lux = 100.0f;
static CCMRAM bool  lux_valid = false;

static EHW_Expr_t check_tcs3472(void) {
  if (!boardTCS3472.isInitialized()) {
    if (!tcs_warned) {
      tcs_warned = true;
      LOG_W("EHW", "TCS3472 not init");
    }
    return EHW_EXPR_NONE;
  }

  uint32_t t0 = HAL_GetTick();
  TCS3472_ColorData_t c = boardTCS3472.readColor();
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "TCS3472 read took %lums", (unsigned long)dt);

  if (c.lux < 0.0f || c.lux > 20000.0f) return EHW_EXPR_NONE;
  if (!lux_valid) {
    smooth_lux = c.lux;
    lux_valid = true;
  } else {
    smooth_lux = smooth_lux * 0.88f + c.lux * 0.12f;
  }
  float lux = smooth_lux;

  if (stable_expr == EHW_EXPR_DARK) {
    if (lux > 26.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_DARK;
  }
  if (stable_expr == EHW_EXPR_BRIGHT) {
    if (lux < 430.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_BRIGHT;
  }

  if (lux < 7.0f) return EHW_EXPR_DARK;
  if (lux > 760.0f) return EHW_EXPR_BRIGHT;
  return EHW_EXPR_NONE;
}

// ============ Public API ============

void EHW_Init(void) {
  last_expr = EHW_EXPR_NONE;
  stable_expr = EHW_EXPR_NONE;
  pending_expr = EHW_EXPR_NONE;
  last_poll = 0;
  expr_since = HAL_GetTick();
  cooldown_until = 0;
  next_comfy_due = HAL_GetTick() + 9000U;
  pending_cnt = 0;
  none_cnt = 0;

  grav_x = 0.0f;
  grav_y = 0.0f;
  grav_z = 1.0f;
  smooth_motion = 0.0f;
  smooth_gyro = 0.0f;
  smooth_tilt = 0.0f;
  smooth_tilt_rate = 0.0f;
  last_pitch = 0.0f;
  last_roll = 0.0f;
  imu_valid = false;
  temp_valid = false;
  lux_valid = false;
  smooth_temp = 22.0f;
  smooth_lux = 100.0f;
  jy_warned = bmp_warned = tcs_warned = false;

  LOG_I("EHW", "Init done");
}

EHW_Expr_t EHW_Update(void) {
  uint32_t now = HAL_GetTick();
  if (now - last_poll < POLL_MS) return last_expr;
  last_poll = now;

  EHW_Expr_t raw = check_jy901s();
  if (raw == EHW_EXPR_NONE) raw = check_tcs3472();
  if (raw == EHW_EXPR_NONE) raw = check_bmp180(now);

  // Cooldown is only for slow environment expressions.  Motion/gravity must
  // preempt immediately; otherwise random moods can make the IMU feel broken.
  if (now < cooldown_until && raw != stable_expr && !is_motion_expr(raw)) {
    raw = EHW_EXPR_NONE;
  }

  if (raw == EHW_EXPR_NONE) {
    if (stable_expr != EHW_EXPR_NONE) {
      none_cnt++;
      if (none_cnt >= DEBOUNCE_OFF && now - expr_since >= MIN_HOLD_MS) {
        EHW_Expr_t old_expr = stable_expr;
        LOG_I("EHW", "expr: %d -> NONE", stable_expr);
        stable_expr = EHW_EXPR_NONE;
        pending_expr = EHW_EXPR_NONE;
        pending_cnt = 0;
        expr_since = now;
        cooldown_until = now + (is_motion_expr(old_expr) ? MOTION_COOLDOWN_MS : COOLDOWN_MS);
      }
    } else {
      none_cnt = 0;
    }
    last_expr = stable_expr;
    return last_expr;
  }

  none_cnt = 0;
  if (raw == pending_expr) {
    pending_cnt++;
  } else {
    pending_expr = raw;
    pending_cnt = 1;
  }

  int need = (raw == EHW_EXPR_DIZZY || raw == EHW_EXPR_PETTED) ? 1 : DEBOUNCE_ON;
  uint32_t hold_ms = is_motion_expr(raw) ? MOTION_HOLD_MS : MIN_HOLD_MS;
  if (pending_cnt >= need && raw != stable_expr && now - expr_since >= hold_ms) {
    LOG_I("EHW", "expr: %d -> %d (cnt=%d)", stable_expr, raw, pending_cnt);
    stable_expr = raw;
    expr_since = now;
    if (raw == EHW_EXPR_COMFY) {
      next_comfy_due = now + COMFY_PERIOD_MS;
    }
  }

  last_expr = stable_expr;
  return last_expr;
}

EHW_Expr_t EHW_GetExpr(void) {
  return last_expr;
}
