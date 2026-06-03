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
static CCMRAM EHW_Expr_t last_raw_expr = EHW_EXPR_NONE;
static CCMRAM uint32_t   last_poll     = 0;
static CCMRAM uint32_t   expr_since    = 0;
static CCMRAM uint32_t   cooldown_until = 0;
static CCMRAM uint32_t   next_comfy_due = 0;
static CCMRAM uint32_t   dizzy_ended_at = 0;
static CCMRAM int        pending_cnt   = 0;
static CCMRAM int        none_cnt      = 0;

#define POLL_MS          90U
#define DEBOUNCE_ON      2
#define DEBOUNCE_OFF     3
#define MIN_HOLD_MS      1200U
#define MOTION_HOLD_MS   120U
#define PETTED_HOLD_MS   480U
#define PETTED_DEBOUNCE_ON 5
#define COOLDOWN_MS      1200U
#define MOTION_COOLDOWN_MS 120U
#define DIZZY_PET_SUPPRESS_MS 900U
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
static CCMRAM float rest_pitch = 0.0f;
static CCMRAM float rest_roll = 0.0f;
static CCMRAM uint32_t last_imu_sample = 0;
static CCMRAM uint32_t next_imu_diag = 0;
static CCMRAM bool  imu_valid = false;
static CCMRAM bool  rest_valid = false;
static CCMRAM bool  jy_warned = false;
static CCMRAM bool  bmp_warned = false;
static CCMRAM bool  tcs_warned = false;

/* Sensor bus watchdogs. Hot-plugging / brownouts around ESP8266 can leave I2C
 * slaves in a state where every read waits for HAL timeout (~100 ms).  If we
 * keep polling them every pet frame, UI/buttons look frozen while IWDG is still
 * fed.  After a few slow/bad reads, temporarily suspend that sensor and retry
 * later at low frequency. */
static CCMRAM uint32_t jy_suspend_until = 0;
static CCMRAM uint32_t tcs_suspend_until = 0;
static CCMRAM uint32_t bmp_suspend_until = 0;
static CCMRAM uint32_t jy_next_retry_log = 0;
static CCMRAM uint32_t tcs_next_retry_log = 0;
static CCMRAM uint32_t bmp_next_retry_log = 0;
static CCMRAM uint8_t  jy_slow_cnt = 0;
static CCMRAM uint8_t  tcs_slow_cnt = 0;
static CCMRAM uint8_t  bmp_slow_cnt = 0;

#define SENSOR_SLOW_LIMIT_MS   95U
#define SENSOR_SUSPEND_MS      30000U
#define SENSOR_BAD_SUSPEND_MS  15000U
#define SENSOR_SLOW_LIMIT_CNT  3U

static bool sensor_suspended(uint32_t now, uint32_t until, uint32_t *next_log, const char *name) {
  if ((int32_t)(now - until) < 0) {
    if (next_log && (int32_t)(now - *next_log) >= 0) {
      *next_log = now + 5000U;
      LOG_W("EHW", "%s suspended, retry in %lums", name, (unsigned long)(until - now));
    }
    return true;
  }
  return false;
}

static void sensor_note_ok(uint8_t *cnt) {
  if (cnt) *cnt = 0;
}

static void sensor_note_slow(uint32_t now, uint32_t dt, uint8_t *cnt,
                             uint32_t *until, uint32_t *next_log, const char *name) {
  if (dt <= SENSOR_SLOW_LIMIT_MS) {
    sensor_note_ok(cnt);
    return;
  }
  if (cnt && *cnt < 255U) (*cnt)++;
  LOG_W("EHW", "%s read took %lums", name, (unsigned long)dt);
  if (cnt && *cnt >= SENSOR_SLOW_LIMIT_CNT) {
    if (until) *until = now + SENSOR_SUSPEND_MS;
    if (next_log) *next_log = now + 5000U;
    *cnt = 0;
    LOG_W("EHW", "%s disabled for %lums after repeated timeout",
          name, (unsigned long)SENSOR_SUSPEND_MS);
  }
}

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
  uint32_t now = HAL_GetTick();
  if (sensor_suspended(now, jy_suspend_until, &jy_next_retry_log, "JY901S")) {
    return EHW_EXPR_NONE;
  }

  if (!boardJY901S.isInitialized()) {
    if (!jy_warned) {
      jy_warned = true;
      LOG_W("EHW", "JY901S not init");
    }
    return EHW_EXPR_NONE;
  }

  uint32_t t0 = now;
  JY901S_Data_t d = boardJY901S.readData();
  uint32_t dt = HAL_GetTick() - t0;
  sensor_note_slow(now, dt, &jy_slow_cnt, &jy_suspend_until, &jy_next_retry_log, "JY901S");
  if ((int32_t)(HAL_GetTick() - jy_suspend_until) < 0) {
    smooth_motion *= 0.45f;
    smooth_gyro *= 0.55f;
    smooth_tilt *= 0.70f;
    smooth_tilt_rate *= 0.45f;
    return EHW_EXPR_NONE;
  }

  float acc_mag = sqrtf(d.acc_x * d.acc_x + d.acc_y * d.acc_y + d.acc_z * d.acc_z);
  if (acc_mag < 0.001f || acc_mag > 400.0f) {
    // Bad read / disconnected bus burst. Decay instead of creating a fake mood.
    if (now >= next_imu_diag) {
      next_imu_diag = now + 3000U;
      LOG_W("EHW", "IMU bad acc mag=%d", (int)(acc_mag * 100.0f));
    }
    if (++jy_slow_cnt >= SENSOR_SLOW_LIMIT_CNT) {
      jy_suspend_until = now + SENSOR_BAD_SUSPEND_MS;
      jy_next_retry_log = now + 5000U;
      jy_slow_cnt = 0;
      LOG_W("EHW", "JY901S disabled for %lums after bad samples",
            (unsigned long)SENSOR_BAD_SUSPEND_MS);
    }
    smooth_motion *= 0.55f;
    smooth_gyro *= 0.65f;
    smooth_tilt *= 0.88f;
    smooth_tilt_rate *= 0.50f;
    return EHW_EXPR_NONE;
  }
  sensor_note_ok(&jy_slow_cnt);

  // Normalize the acceleration vector before using it as gravity.  JY901S can
  // be configured for different acceleration ranges; normalized tilt is stable
  // even if the raw unit is g or m/s^2.
  float nx = d.acc_x / acc_mag;
  float ny = d.acc_y / acc_mag;
  float nz = d.acc_z / acc_mag;

  uint32_t sample_dt_ms = last_imu_sample ? (now - last_imu_sample) : POLL_MS;
  if (sample_dt_ms < 1U) sample_dt_ms = 1U;
  if (sample_dt_ms > 500U) sample_dt_ms = 500U;
  last_imu_sample = now;
  float sample_dt_s = (float)sample_dt_ms * 0.001f;

  float roll_acc = atan2f(ny, nz) * 57.29578f;
  float pitch_acc = atan2f(-nx, sqrtf(ny * ny + nz * nz)) * 57.29578f;
  float roll = roll_acc;
  float pitch = pitch_acc;
  if (fabsf(d.roll) <= 180.0f && fabsf(d.pitch) <= 180.0f &&
      (fabsf(d.roll) > 0.01f || fabsf(d.pitch) > 0.01f)) {
    // The module's angle estimate is steadier during gentle handling; keep a
    // little accelerometer authority so gravity still wins after odd packets.
    roll = d.roll * 0.70f + roll_acc * 0.30f;
    pitch = d.pitch * 0.70f + pitch_acc * 0.30f;
  }

  float lin_mag = 0.0f;
  if (!imu_valid) {
    grav_x = nx;
    grav_y = ny;
    grav_z = nz;
    last_roll = roll;
    last_pitch = pitch;
    rest_roll = roll;
    rest_pitch = pitch;
    rest_valid = true;
    imu_valid = true;
  } else {
    float lin_x = nx - grav_x;
    float lin_y = ny - grav_y;
    float lin_z = nz - grav_z;
    lin_mag = sqrtf(lin_x * lin_x + lin_y * lin_y + lin_z * lin_z);

    // A slow gravity low-pass keeps short bumps from becoming "new down".
    float grav_alpha = lin_mag > 0.12f ? 0.08f : 0.18f;
    grav_x = grav_x * (1.0f - grav_alpha) + nx * grav_alpha;
    grav_y = grav_y * (1.0f - grav_alpha) + ny * grav_alpha;
    grav_z = grav_z * (1.0f - grav_alpha) + nz * grav_alpha;
  }

  float gyro_mag = sqrtf(d.gyro_x * d.gyro_x + d.gyro_y * d.gyro_y + d.gyro_z * d.gyro_z);

  float roll_delta = fabsf(angle_diff(roll, last_roll));
  float pitch_delta = fabsf(angle_diff(pitch, last_pitch));
  float tilt_rate = (roll_delta + pitch_delta) / sample_dt_s;
  last_roll = roll;
  last_pitch = pitch;

  if (!rest_valid) {
    rest_roll = roll;
    rest_pitch = pitch;
    rest_valid = true;
  }
  float tilt_from_rest = fmaxf(fabsf(angle_diff(roll, rest_roll)),
                              fabsf(angle_diff(pitch, rest_pitch)));
  if (lin_mag < 0.025f && gyro_mag < 2.8f && tilt_rate < 7.0f && tilt_from_rest < 10.0f) {
    rest_roll = rest_roll * 0.998f + roll * 0.002f;
    rest_pitch = rest_pitch * 0.998f + pitch * 0.002f;
  }

  smooth_motion = smooth_motion * 0.62f + lin_mag * 0.38f;
  smooth_gyro = smooth_gyro * 0.58f + gyro_mag * 0.42f;
  smooth_tilt = smooth_tilt * 0.72f + tilt_from_rest * 0.28f;
  smooth_tilt_rate = smooth_tilt_rate * 0.58f + tilt_rate * 0.42f;

  if (now >= next_imu_diag &&
      (stable_expr != EHW_EXPR_NONE || smooth_motion > 0.035f ||
       smooth_tilt > 8.0f || smooth_tilt_rate > 10.0f)) {
    next_imu_diag = now + 3000U;
    LOG_D("EHW", "imu acc=%d lin=%d gyro=%d rawg=%d tilt=%d rate=%d rest=%d",
          (int)(acc_mag * 100.0f),
          (int)(smooth_motion * 1000.0f),
          (int)(smooth_gyro * 10.0f),
          (int)gyro_mag,
          (int)(smooth_tilt * 10.0f),
          (int)smooth_tilt_rate,
          rest_valid ? 1 : 0);
  }

  // Thresholds calibrated against real JY901S data.
  // Smoothed gyro converges too quickly (EMA α=0.42) to distinguish handling
  // from shaking — both end up in the 3-7 range.  Raw gyro_mag (instantaneous,
  // deg/s) spikes much higher during real shake, so violent detection uses it
  // directly while gentle_touch still uses the smoothed values for stability.
  bool violent_motion = (smooth_motion > 0.48f || gyro_mag > 180.0f ||
                         smooth_tilt_rate > 175.0f ||
                         (smooth_motion > 0.35f && smooth_tilt_rate > 120.0f));
  bool dizzy_tail = (smooth_motion > 0.32f || smooth_gyro > 4.5f ||
                     smooth_tilt_rate > 120.0f || gyro_mag > 100.0f);
  bool gentle_touch = (smooth_motion > 0.22f || smooth_gyro > 3.5f ||
                       smooth_tilt_rate > 80.0f || smooth_tilt > 3.5f);
  bool gentle_tail = (smooth_motion > 0.13f || smooth_gyro > 2.8f ||
                      smooth_tilt_rate > 50.0f || smooth_tilt > 2.6f);

  // Violent motion wins even while PETTED is stable.  The previous ordering
  // let a shake be swallowed by the PETTED hysteresis branch.
  if (violent_motion) {
    return EHW_EXPR_DIZZY;
  }

  // Hysteresis while an expression is active.  This avoids flicker but does not
  // block real gravity/IMU events behind the random emotion loop.
  if (stable_expr == EHW_EXPR_DIZZY) {
    if (dizzy_tail) {
      return EHW_EXPR_DIZZY;
    }
  }
  if (stable_expr == EHW_EXPR_PETTED) {
    if (gentle_tail && !dizzy_tail) {
      return EHW_EXPR_PETTED;
    }
  }

  // Gentle tilt / touch / hand movement: petted.  Tilt is measured from the
  // learned rest posture, so a slightly angled installation does not trigger
  // forever, but a real hand tilt still feels immediate.
  //
  // After a shake ends, suppress PETTED briefly so the same handling that
  // caused DIZZY doesn't immediately re-trigger a conflicting expression.
  if (gentle_touch && !dizzy_tail) {
    if (now - dizzy_ended_at < DIZZY_PET_SUPPRESS_MS) return EHW_EXPR_NONE;
    return EHW_EXPR_PETTED;
  }

  return EHW_EXPR_NONE;
}

// ============ BMP180 temperature ============

static CCMRAM float smooth_temp = 22.0f;
static CCMRAM bool  temp_valid = false;

static EHW_Expr_t check_bmp180(uint32_t now) {
  if (sensor_suspended(now, bmp_suspend_until, &bmp_next_retry_log, "BMP180")) {
    return EHW_EXPR_NONE;
  }

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
  sensor_note_slow(now, dt, &bmp_slow_cnt, &bmp_suspend_until, &bmp_next_retry_log, "BMP180");
  if ((int32_t)(HAL_GetTick() - bmp_suspend_until) < 0) return EHW_EXPR_NONE;

  if (temp < -20.0f || temp > 85.0f) return EHW_EXPR_NONE;
  sensor_note_ok(&bmp_slow_cnt);
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
  uint32_t now = HAL_GetTick();
  if (sensor_suspended(now, tcs_suspend_until, &tcs_next_retry_log, "TCS3472")) {
    return EHW_EXPR_NONE;
  }

  if (!boardTCS3472.isInitialized()) {
    if (!tcs_warned) {
      tcs_warned = true;
      LOG_W("EHW", "TCS3472 not init");
    }
    return EHW_EXPR_NONE;
  }

  uint32_t t0 = now;
  TCS3472_ColorData_t c = boardTCS3472.readColor();
  uint32_t dt = HAL_GetTick() - t0;
  sensor_note_slow(now, dt, &tcs_slow_cnt, &tcs_suspend_until, &tcs_next_retry_log, "TCS3472");
  if ((int32_t)(HAL_GetTick() - tcs_suspend_until) < 0) return EHW_EXPR_NONE;

  if (c.lux < 0.0f || c.lux > 20000.0f) return EHW_EXPR_NONE;
  sensor_note_ok(&tcs_slow_cnt);
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
  last_raw_expr = EHW_EXPR_NONE;
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
  rest_pitch = 0.0f;
  rest_roll = 0.0f;
  last_imu_sample = 0;
  next_imu_diag = HAL_GetTick() + 3000U;
  imu_valid = false;
  rest_valid = false;
  temp_valid = false;
  lux_valid = false;
  smooth_temp = 22.0f;
  smooth_lux = 100.0f;
  jy_warned = bmp_warned = tcs_warned = false;
  jy_suspend_until = tcs_suspend_until = bmp_suspend_until = 0;
  jy_next_retry_log = tcs_next_retry_log = bmp_next_retry_log = 0;
  jy_slow_cnt = tcs_slow_cnt = bmp_slow_cnt = 0;

  LOG_I("EHW", "Init done, poll=%ums motion_hold=%ums", POLL_MS, MOTION_HOLD_MS);
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

  if (raw != last_raw_expr) {
    LOG_D("EHW", "raw: %d -> %d stable=%d pending=%d cnt=%d",
          last_raw_expr, raw, stable_expr, pending_expr, pending_cnt);
    last_raw_expr = raw;
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
        if (old_expr == EHW_EXPR_DIZZY) dizzy_ended_at = now;
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

  int need = raw == EHW_EXPR_DIZZY ? 1 :
             (raw == EHW_EXPR_PETTED ? PETTED_DEBOUNCE_ON : DEBOUNCE_ON);
  uint32_t hold_ms = raw == EHW_EXPR_DIZZY ? MOTION_HOLD_MS :
                     (raw == EHW_EXPR_PETTED ? PETTED_HOLD_MS : MIN_HOLD_MS);
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
