/**
 ******************************************************************************
 * @file    emotion_manager.cpp
 * @author  Typheye
 * @brief   Emotion Manager implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#include "include/emotion_manager.h"


static char g_manual_expr[16];
static char g_auto_expr[16];
static bool g_manual_mode = false;
static uint32_t g_manual_until = 0;
static uint32_t g_revision = 0;

#define EMOTION_MANUAL_TTL_MS 7500U

static bool time_due(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

static void expire_manual_if_needed(void) {
  if (g_manual_mode && g_manual_until != 0 && time_due(HAL_GetTick(), g_manual_until)) {
    g_manual_mode = false;
    g_manual_until = 0;
    g_revision++;
    LOG_D("EMGR", "Manual expression expired, back to auto");
  }
}

static const char *expr_normalize(const char *expr) {
  if (!expr || !expr[0]) return "idle";
  if (strcmp(expr, "angry") == 0 || strcmp(expr, "mad") == 0) return "annoyed";
  if (strcmp(expr, "normal") == 0 || strcmp(expr, "neutral") == 0) return "idle";
  if (strcmp(expr, "surprise") == 0) return "surprised";
  if (strcmp(expr, "cute") == 0) return "shy";
  return expr;
}

static bool expr_allowed(const char *expr) {
  expr = expr_normalize(expr);
  if (!expr || !expr[0]) return false;

  static const char *kAllowed[] = {
      "idle",      "happy", "sad",  "confused", "surprised",
      "love",      "sleepy", "dizzy", "petted",   "shy",
      "annoyed",   "curious"};

  for (unsigned i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); ++i) {
    if (strcmp(expr, kAllowed[i]) == 0) return true;
  }
  return false;
}

static void copy_expr(char *dst, const char *src) {
  src = expr_normalize(src);
  if (!src || !src[0]) src = "idle";
  strncpy(dst, src, 15);
  dst[15] = '\0';
}

void EmotionManager_Init(void) {
  copy_expr(g_manual_expr, "idle");
  copy_expr(g_auto_expr, "idle");
  g_manual_mode = false;
  g_manual_until = 0;
  g_revision = 1;
}

bool EmotionManager_SetExpression(const char *expr) {
  const char *norm = expr_normalize(expr);
  if (strcmp(norm, "auto") == 0 || strcmp(norm, "idle") == 0) {
    EmotionManager_SetAuto();
    return true;
  }

  if (!expr_allowed(norm)) {
    LOG_W("EMGR", "Reject expression: %s", expr ? expr : "(null)");
    return false;
  }

  copy_expr(g_manual_expr, norm);
  g_manual_mode = true;
  g_manual_until = HAL_GetTick() + EMOTION_MANUAL_TTL_MS;
  /* Increment even if the same expression is sent again; a repeated cloud
   * command should be visible, but the launcher must consume it only once. */
  g_revision++;
  LOG_I("EMGR", "Manual expression: %s", g_manual_expr);
  return true;
}

void EmotionManager_SetAuto(void) {
  if (g_manual_mode || strcmp(g_manual_expr, "idle") != 0) {
    g_revision++;
    LOG_I("EMGR", "Expression mode: auto");
  }
  copy_expr(g_manual_expr, "idle");
  g_manual_mode = false;
  g_manual_until = 0;
}

bool EmotionManager_IsManual(void) {
  expire_manual_if_needed();
  return g_manual_mode;
}

const char *EmotionManager_GetExpression(void) {
  expire_manual_if_needed();
  return g_manual_mode ? g_manual_expr : g_auto_expr;
}

const char *EmotionManager_GetReportExpression(void) {
  expire_manual_if_needed();
  return g_manual_mode ? g_manual_expr : g_auto_expr;
}

uint32_t EmotionManager_GetRevision(void) {
  expire_manual_if_needed();
  return g_revision;
}

void EmotionManager_ReportAutoExpression(const char *expr) {
  if (g_manual_mode || !expr || !expr[0]) return;
  if (strcmp(g_auto_expr, expr) == 0) return;
  copy_expr(g_auto_expr, expr);
  g_revision++;
}

void EmotionManager_Tick(void) {
  expire_manual_if_needed();
}
