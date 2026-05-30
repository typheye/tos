#include "include/libehw.h"
#include "include/jy901s.hpp"
#include "include/bmp180.hpp"
#include "include/tcs3472.hpp"
#include <math.h>
#include <stdio.h>
#include "syslog.h"

#ifndef CCMRAM
#define CCMRAM __attribute__((section(".ccmram")))
#endif

static CCMRAM EHW_Expr_t last_expr    = EHW_EXPR_NONE;
static CCMRAM EHW_Expr_t stable_expr  = EHW_EXPR_NONE;
static CCMRAM EHW_Expr_t pending_expr = EHW_EXPR_NONE;
static CCMRAM uint32_t  last_poll     = 0;
static CCMRAM uint32_t  expr_since    = 0;
static CCMRAM uint32_t  cooldown_until = 0;
static CCMRAM int       pending_cnt   = 0;

#define POLL_MS      500
#define DEBOUNCE     4
#define MIN_HOLD_MS  3000
#define COOLDOWN_MS  5000
#define SENSOR_TO_MS 100   // max ms a single sensor read may take

// ============ JY901S motion detection ============

static CCMRAM float smooth_delta = 0.0f;
static CCMRAM float last_mag     = 0.0f;
static CCMRAM bool  mag_valid    = false;

static EHW_Expr_t check_jy901s(void) {
  if (!boardJY901S.isInitialized()) { LOG_W("EHW", "JY901S not init"); return EHW_EXPR_NONE; }

  uint32_t t0 = HAL_GetTick();
  JY901S_Data_t d = boardJY901S.readData();
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "JY901S read took %lums", (unsigned long)dt);

  float mag = sqrtf(d.acc_x * d.acc_x + d.acc_y * d.acc_y + d.acc_z * d.acc_z);
  float gyro_mag = sqrtf(d.gyro_x * d.gyro_x + d.gyro_y * d.gyro_y + d.gyro_z * d.gyro_z);

  float delta = 0.0f;
  if (mag_valid) delta = fabsf(mag - last_mag);
  last_mag  = mag;
  mag_valid = true;

  if (delta < 0.2f)
    smooth_delta *= 0.35f;
  else
    smooth_delta = smooth_delta * 0.5f + delta * 0.5f;

  if (stable_expr == EHW_EXPR_DIZZY) {
    if (smooth_delta < 0.6f && gyro_mag < 80.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_DIZZY;
  }
  if (smooth_delta > 2.5f || gyro_mag > 250.0f)
    return EHW_EXPR_DIZZY;

  if (stable_expr == EHW_EXPR_PETTED) {
    if (smooth_delta < 0.5f && gyro_mag < 50.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_PETTED;
  }
  if (smooth_delta > 1.2f || gyro_mag > 100.0f)
    return EHW_EXPR_PETTED;

  return EHW_EXPR_NONE;
}

// ============ BMP180 temperature ============

static CCMRAM float smooth_temp = 22.0f;

static EHW_Expr_t check_bmp180(void) {
  if (!boardBMP180.isInitialized()) { LOG_W("EHW", "BMP180 not init"); return EHW_EXPR_NONE; }

  uint32_t t0 = HAL_GetTick();
  float temp = boardBMP180.readTemperature();  // faster, no pressure hang risk
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "BMP180 read took %lums", (unsigned long)dt);

  smooth_temp = smooth_temp * 0.85f + temp * 0.15f;
  float t = smooth_temp;

  if (stable_expr == EHW_EXPR_COLD) {
    if (t > 18.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_COLD;
  }
  if (stable_expr == EHW_EXPR_HOT) {
    if (t < 32.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_HOT;
  }
  if (stable_expr == EHW_EXPR_COMFY) {
    if (t < 20.0f || t > 30.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_COMFY;
  }

  if (t < 14.0f)  return EHW_EXPR_COLD;
  if (t > 36.0f)  return EHW_EXPR_HOT;
  if (t >= 21.0f && t <= 29.0f) return EHW_EXPR_COMFY;
  return EHW_EXPR_NONE;
}

// ============ TCS3472 ambient light ============

static CCMRAM float smooth_lux = 100.0f;

static EHW_Expr_t check_tcs3472(void) {
  if (!boardTCS3472.isInitialized()) { LOG_W("EHW", "TCS3472 not init"); return EHW_EXPR_NONE; }

  uint32_t t0 = HAL_GetTick();
  TCS3472_ColorData_t c = boardTCS3472.readColor();
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > SENSOR_TO_MS) LOG_W("EHW", "TCS3472 read took %lums", (unsigned long)dt);

  smooth_lux = smooth_lux * 0.9f + c.lux * 0.1f;
  float lux = smooth_lux;

  if (stable_expr == EHW_EXPR_DARK) {
    if (lux > 20.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_DARK;
  }
  if (stable_expr == EHW_EXPR_BRIGHT) {
    if (lux < 300.0f) return EHW_EXPR_NONE;
    return EHW_EXPR_BRIGHT;
  }

  if (lux < 8.0f)   return EHW_EXPR_DARK;
  if (lux > 600.0f) return EHW_EXPR_BRIGHT;
  return EHW_EXPR_NONE;
}

// ============ Public API ============

void EHW_Init(void) {
  smooth_delta = 0.0f;
  smooth_temp  = 22.0f;
  smooth_lux   = 100.0f;
  last_mag     = 0.0f;
  mag_valid    = false;
  last_expr    = EHW_EXPR_NONE;
  stable_expr  = EHW_EXPR_NONE;
  pending_expr = EHW_EXPR_NONE;
  last_poll    = 0;
  pending_cnt  = 0;
  expr_since   = 0;
  cooldown_until = 0;
  LOG_I("EHW", "Init done");
}

EHW_Expr_t EHW_Update(void) {
  uint32_t now = HAL_GetTick();
  if (now - last_poll < POLL_MS) return last_expr;
  last_poll = now;

  EHW_Expr_t raw = check_jy901s();
  if (raw == EHW_EXPR_NONE) raw = check_bmp180();
  if (raw == EHW_EXPR_NONE) raw = check_tcs3472();

  if (raw == pending_expr) {
    pending_cnt++;
  } else {
    pending_expr = raw;
    pending_cnt = 1;
  }

  if (pending_cnt >= DEBOUNCE && pending_expr != stable_expr) {
    if (now - expr_since >= MIN_HOLD_MS && now >= cooldown_until) {
      LOG_I("EHW", "expr: %d -> %d (cnt=%d)", stable_expr, pending_expr, pending_cnt);
      stable_expr = pending_expr;
      expr_since = now;
      if (pending_expr == EHW_EXPR_NONE) {
        cooldown_until = now + COOLDOWN_MS;
      }
    }
  }

  last_expr = stable_expr;
  return last_expr;
}

EHW_Expr_t EHW_GetExpr(void) {
  return last_expr;
}
