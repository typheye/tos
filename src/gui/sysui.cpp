/**
 ******************************************************************************
 * @file    sysui.cpp
 * @author  Typheye
 * @brief   System UI registry implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "include/sysui.hpp"
#include "dram.h"
#include "library/include/libdly.h"
#include "library/include/libui.h"

extern LCD boardLCD;
extern TRTC boardTRTC;
extern KeyManager keyManager;

int SysUI::_nowActivity = UI_DASHBOARD;
uint32_t SysUI::_lastTick = 0;
void (*SysUI::_currentTestFunc)(void) = nullptr;
int SysUI::_cpuUsage = 0;

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
  snprintf(g_dbg_line_perf, sizeof(g_dbg_line_perf), "FPS: %3uF | CPU: %3u%%",
           (unsigned)fps, (unsigned)g_dbg_cpu_pct);
  snprintf(g_dbg_line_mem, sizeof(g_dbg_line_mem), "RAM: %3u%% | CRM: %3u%%",
           (unsigned)g_dbg_ram_pct, (unsigned)g_dbg_cram_pct);
}

static void debug_sample_memory(void) {
  SysDramStats_t ram;
  SysDramStats_t ccm;
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
  uint32_t ram_static = ram_total > ram.total ? ram_total - ram.total : 0U;
  uint32_t cram_static = cram_total > ccm.total ? cram_total - ccm.total : 0U;
  g_dbg_ram_used = ram_static + ram.used;
  g_dbg_cram_used = cram_static + ccm.used;
  if (g_dbg_ram_used > ram_total)
    g_dbg_ram_used = ram_total;
  if (g_dbg_cram_used > cram_total)
    g_dbg_cram_used = cram_total;

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

void SysUI::init(void) { _lastTick = HAL_GetTick(); }

void SysUI::loop(void) {
  TosApi_SetPaused(_nowActivity != UI_PET);
  TosApi_Tick();

  Time_t now;
  Date_t today;
  boardTRTC.getDateTime(&now, &today);
  char time_str[8];
  SysTime_Fmt(time_str, sizeof(time_str), now.hours, now.minutes);
  PD_SetHeaderTime(time_str);

  if (_nowActivity == UI_LAUNCHER) {
    static int sel = 0;
    sel = UI_MenuLoop("TOS", tos_m, TOS_ITEMS, sel);
    if (sel == 0) {
      _nowActivity = UI_PET;
    } else if (sel == 1) {
      settings_run();
    } else if (sel == 2) {
      file_manager_run();
    } else if (sel == 3) {
      hid_tools_run();
    } else if (sel == 4) {
      demo_list_run();
    }
  } else if (_nowActivity == UI_RUNNING_TEST) {
    runCurrentTest();
  } else if (_nowActivity == UI_PET) {
    petLauncherRun();
    _nowActivity = UI_LAUNCHER;
    TosApi_SetPaused(true);
  }
}

void SysUI::runCurrentTest(void) {
  if (_currentTestFunc) {
    _currentTestFunc();
  }
  _nowActivity = UI_LAUNCHER;
  _currentTestFunc = nullptr;
}

void SysUI::setActivity(int a) { _nowActivity = a; }
int SysUI::getActivity(void) { return _nowActivity; }
void SysUI::setCurrentTest(void (*f)(void)) { _currentTestFunc = f; }
void SysUI::resetMenuPosition(void) {}
void SysUI::updateCpuUsage(uint32_t w) { _cpuUsage = (int)w; }
