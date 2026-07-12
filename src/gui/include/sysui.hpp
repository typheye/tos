/**
 ******************************************************************************
 * @file    sysui.hpp
 * @author  Typheye
 * @brief   Sysui interface.
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

#ifndef SYSUI_HPP
#define SYSUI_HPP

#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "library/include/libpd.h"
#include "main.h"
#include <cstdio>
#include <cstring>
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/systime.h"
#include "gui/demo/include/demo_activity.hpp"
#include "gui/miniapp/hid_tools/include/app.h"
#include "gui/include/settings.hpp"
#include "gui/miniapp/file_manager/include/app.h"
#include "hardware/include/trtc.hpp"
#include "gui/include/launcher.hpp"
#include "include/libpd.h"
#include "core/sys/include/syslog.h"

#define UI_DASHBOARD    0
#define UI_LAUNCHER     1
#define UI_RUNNING_TEST 2
#define UI_PET          3

class SysUI {
public:
  static void init(void);
  static void loop(void);
  static void setActivity(int activity);
  static int getActivity(void);
  static void setCurrentTest(void (*test_func)(void));
  static void resetMenuPosition(void);
  static void updateCpuUsage(uint32_t work_ms);

private:
  static int _nowActivity;
  static uint32_t _lastTick;
  static void (*_currentTestFunc)(void);
  static int _cpuUsage;

  static void drawLauncher(void);
  static void handleLauncherInput(void);
  static void runCurrentTest(void);
};

#ifdef __cplusplus
extern "C" {
#endif
void SysUI_DebugOverlayBeginFrame(void);
void SysUI_DebugOverlayEndFrame(void);
void SysUI_DebugOverlayDraw(void);
void SysUI_DebugOverlaySetEnabled(uint8_t enabled);
uint8_t SysUI_DebugOverlayIsEnabled(void);
#ifdef __cplusplus
}
#endif

#endif
