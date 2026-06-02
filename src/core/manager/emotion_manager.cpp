/**
 * @file    emotion_manager.cpp
 * @brief   Central expression state shared by launcher and cloud API.
 */

#include "include/emotion_manager.h"
#include "syslog.h"
#include <cstdio>
#include <cstring>

static char g_manual_expr[16];
static char g_auto_expr[16];
static bool g_manual_mode = false;
static uint32_t g_revision = 0;

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
  g_revision = 1;
}

bool EmotionManager_SetExpression(const char *expr) {
  if (!expr_allowed(expr)) {
    LOG_W("EMGR", "Reject expression: %s", expr ? expr : "(null)");
    return false;
  }

  bool changed = (!g_manual_mode || strcmp(g_manual_expr, expr) != 0);
  copy_expr(g_manual_expr, expr);
  g_manual_mode = true;
  if (changed) {
    g_revision++;
    LOG_I("EMGR", "Manual expression: %s", g_manual_expr);
  }
  return true;
}

void EmotionManager_SetAuto(void) {
  if (g_manual_mode) {
    g_revision++;
    LOG_I("EMGR", "Expression mode: auto");
  }
  g_manual_mode = false;
}

bool EmotionManager_IsManual(void) { return g_manual_mode; }

const char *EmotionManager_GetExpression(void) {
  return g_manual_mode ? g_manual_expr : g_auto_expr;
}

const char *EmotionManager_GetReportExpression(void) {
  return g_manual_mode ? g_manual_expr : g_auto_expr;
}

uint32_t EmotionManager_GetRevision(void) { return g_revision; }

void EmotionManager_ReportAutoExpression(const char *expr) {
  if (g_manual_mode || !expr || !expr[0]) return;
  if (strcmp(g_auto_expr, expr) == 0) return;
  copy_expr(g_auto_expr, expr);
  g_revision++;
}

void EmotionManager_Tick(void) {
  /* Reserved for future timed expression fades / TTLs. */
}
