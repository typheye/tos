/**
 ******************************************************************************
 * @file    sysui.cpp
 * @author  Typheye
 * @brief   System UI registry implementation.
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

#include "include/sysui.hpp"
#include "core/sys/include/sysdram.h"
#include "library/include/libdly.h"

extern LCD boardLCD;
extern TRTC boardTRTC;
extern KeyManager keyManager;

int SysUI::now_activity = UI_DASHBOARD;
uint32_t SysUI::last_tick = 0;
void (*SysUI::current_test_func)(void) = nullptr;
int SysUI::cpu_usage = 0;

#define TOS_ITEMS 5
static const char *tos_m[TOS_ITEMS] = {"00 Return", "01 Settings",
                                       "02 File Manager", "03 HID Tools",
                                       "04 Demo Activities"};

static uint8_t g_dbg_overlay_enabled = 0;
static uint32_t g_dbg_frame_count = 0;
static uint32_t g_dbg_last_sample = 0;
static uint16_t g_dbg_fps = 0;
static uint32_t g_dbg_ram_used = 0;
static uint32_t g_dbg_cram_used = 0;
static uint8_t g_dbg_ram_pct = 0;
static uint8_t g_dbg_cram_pct = 0;
static uint8_t g_dbg_cpu_pct = 0;
static char g_dbg_line_perf[28] = "FPS:   0F | CPU:   0%";
static char g_dbg_line_mem[28] = "RAM:   0% | CRM:   0%";

static void debug_format_lines(void) {
  uint16_t fps = g_dbg_fps > 999U ? 999U : g_dbg_fps;
  snprintf(g_dbg_line_perf, sizeof(g_dbg_line_perf),
           "FPS: %3uF | CPU: %3u%%", (unsigned)fps,
           (unsigned)g_dbg_cpu_pct);
  snprintf(g_dbg_line_mem, sizeof(g_dbg_line_mem),
           "RAM: %3u%% | CRM: %3u%%", (unsigned)g_dbg_ram_pct,
           (unsigned)g_dbg_cram_pct);
}

static void debug_sample_memory(void) {
  SysDram_Stats_t ram;
  SysDram_Stats_t ccm;
  if (!SysDram_GetStats(SYSDRAM_REGION_RAM, &ram) ||
      !SysDram_GetStats(SYSDRAM_REGION_CCM, &ccm)) {
    g_dbg_ram_used = 0;
    g_dbg_cram_used = 0;
    g_dbg_ram_pct = 0;
    g_dbg_cram_pct = 0;
    return;
  }

  const uint32_t ram_total = 128U * 1024U;
  const uint32_t cram_total = 64U * 1024U;
  g_dbg_ram_used = ram_total > ram.free ? ram_total - ram.free : ram_total;
  g_dbg_cram_used = cram_total > ccm.free ? cram_total - ccm.free : cram_total;

  uint32_t rp = (g_dbg_ram_used * 100U + ram_total / 2U) / ram_total;
  uint32_t cp = (g_dbg_cram_used * 100U + cram_total / 2U) / cram_total;
  g_dbg_ram_pct = (uint8_t)(rp > 100U ? 100U : rp);
  g_dbg_cram_pct = (uint8_t)(cp > 100U ? 100U : cp);
}

extern "C" void SysUI_DebugOverlaySetEnabled(uint8_t enabled) {
  g_dbg_overlay_enabled = enabled ? 1U : 0U;
  g_dbg_frame_count = 0;
  g_dbg_last_sample = HAL_GetTick();
  g_dbg_fps = 0;
  g_dbg_cpu_pct = 0;
  (void)JPDelay_ConsumeIdleMs();
  debug_sample_memory();
  debug_format_lines();
}

extern "C" uint8_t SysUI_DebugOverlayIsEnabled(void) {
  return g_dbg_overlay_enabled;
}

extern "C" void SysUI_DebugOverlayBeginFrame(void) {
  if (!g_dbg_overlay_enabled) {
    return;
  }

  uint32_t now = HAL_GetTick();
  if (g_dbg_last_sample == 0U) {
    g_dbg_last_sample = now;
  }
  g_dbg_frame_count++;
  uint32_t dt = now - g_dbg_last_sample;
  if (dt >= 500U) {
    uint32_t idle = JPDelay_ConsumeIdleMs();
    if (idle > dt) {
      idle = dt;
    }
    uint32_t busy = dt - idle;
    uint32_t pct = (busy * 100U + dt / 2U) / dt;
    g_dbg_cpu_pct = (uint8_t)(pct > 100U ? 100U : pct);
    g_dbg_fps = (uint16_t)((g_dbg_frame_count * 1000U + dt / 2U) / dt);
    g_dbg_frame_count = 0;
    g_dbg_last_sample = now;
    debug_sample_memory();
    debug_format_lines();
  }
}

extern "C" void SysUI_DebugOverlayEndFrame(void) {
  (void)g_dbg_overlay_enabled;
}

extern "C" void SysUI_DebugOverlayDraw(void) {
  if (!g_dbg_overlay_enabled) {
    return;
  }

  PD_SetFont(FONT_ASCII_12);
  uint16_t perf_w = PD_GetStringWidth(g_dbg_line_perf);
  uint16_t mem_w = PD_GetStringWidth(g_dbg_line_mem);
  int16_t w = (int16_t)((perf_w > mem_w ? perf_w : mem_w) + 8U);
  const int16_t h = 29;
  const int16_t x = LCD_WIDTH - w;
  const int16_t y = LCD_HEIGHT - h;

  PD_FillRect(x, y, w, h, 0x000000);

  PD_SetColor(TOS_GREEN);
  PD_DrawString(x + 4, y + 2, g_dbg_line_perf);

  PD_SetColor(TOS_YELLOW);
  PD_DrawString(x + 4, y + 15, g_dbg_line_mem);
}

static void draw_menu(const char *title, const char **items, int count,
                      int sel) {
  LCD_FLUSH({
    PD_Init();
    PD_FillScreen(TOS_BG);
    extern TRTC boardTRTC;
    static uint32_t lt = 0;
    if (HAL_GetTick() - lt > 1000) {
      lt = HAL_GetTick();
      Time_t t;
      Date_t d;
      boardTRTC.getDateTime(&t, &d);
      char ts[8];
      time_fmt(ts, sizeof(ts), t.hours, t.minutes);
      PD_SetHeaderTime(ts);
    }
    PD_DrawFrame();
    PD_SetFont(FONT_ASCII_16);
    PD_SetColor(TOS_ACCENT);
    PD_DrawString(22, 5, title);
    int v = count < 7 ? count : 7;
    int st = sel - v / 2;
    if (st < 0)
      st = 0;
    if (st + v > count)
      st = count - v;
    for (int i = 0; i < v; i++) {
      int idx = st + i;
      if (idx >= count)
        break;
      int cy = 33 + i * 25;
      if (idx == sel) {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_ACCENT);
        PD_SetColor(TOS_TEXT);
      } else {
        PD_DrawAngledCard(14, cy, 212, 20, 5, TOS_CARD_BG);
        PD_SetColor(TOS_TEXT_SEC);
      }
      PD_DrawString(26, cy + 2, items[idx]);
    }
    PD_DrawFooterCenter("ENTER", NULL, "UP/DOWN");
  });
}

static int menu_loop(const char *title, const char **items, int count,
                     int start_sel) {
  int sel = start_sel;
  if (sel >= count)
    sel = 0;
  uint8_t le = 0;
  uint32_t lu = 0;
  while (1) {
    TosApi_Tick();
    keyManager.collision_A8.tick();
    keyManager.collision_D0.tick();
    keyManager.btn_enter.tick();
    if (keyManager.collision_A8.getState() == KEY_PRESSED) {
      sel = (sel + 1) % count;
      JPDelay(45);
    }
    if (keyManager.collision_D0.getState() == KEY_PRESSED) {
      sel = (sel - 1 + count) % count;
      JPDelay(45);
    }
    uint8_t ce = (keyManager.btn_enter.getState() == KEY_PRESSED);
    if (ce && !le) {
      le = ce;
      return sel;
    }
    le = ce;
    if (HAL_GetTick() - lu > 16) {
      lu = HAL_GetTick();
      draw_menu(title, items, count, sel);
    }
    TosApi_Tick();
    JPDelay(1);
  }
}

void SysUI::init(void) { last_tick = HAL_GetTick(); }

void SysUI::loop(void) {
  TosApi_SetPaused(now_activity != UI_PET);
  TosApi_Tick();

  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);
  char time_str[8];
  time_fmt(time_str, sizeof(time_str), now.hours, now.minutes);
  PD_SetHeaderTime(time_str);

  if (now_activity == UI_LAUNCHER) {
    static int sel = 0;
    sel = menu_loop("TOS", tos_m, TOS_ITEMS, sel);
    if (sel == 0) {
      now_activity = UI_PET;
    } else if (sel == 1) {
      settings_run();
    } else if (sel == 2) {
      file_manager_run();
    } else if (sel == 3) {
      hid_tools_run();
    } else if (sel == 4) {
      demo_list_run();
    }
    boardLCD.fillScreen(LCD_COLOR_BLACK);
  } else if (now_activity == UI_RUNNING_TEST) {
    runCurrentTest();
  } else if (now_activity == UI_PET) {
    pet_launcher_run();
    now_activity = UI_LAUNCHER;
    TosApi_SetPaused(true);
  }
}

void SysUI::runCurrentTest(void) {
  if (current_test_func) {
    boardLCD.fillScreen(LCD_COLOR_BLACK);
    current_test_func();
  }
  now_activity = UI_LAUNCHER;
  current_test_func = nullptr;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}

void SysUI::setActivity(int a) {
  now_activity = a;
  boardLCD.fillScreen(LCD_COLOR_BLACK);
}
int SysUI::getActivity(void) { return now_activity; }
void SysUI::setCurrentTest(void (*f)(void)) { current_test_func = f; }
void SysUI::resetMenuPosition(void) {}
void SysUI::updateCpuUsage(uint32_t w) {
  cpu_usage = (int)w;
}
