/**
 ******************************************************************************
 * @file    tos.cpp
 * @author  Typheye
 * @brief   Tos implementation.
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

#include "core/include/tos.hpp"
#include "library/include/libdly.h"


TOS::TOS() : _initialized(false), _tickCount(0) {}

void TOS::start() {
  if (!_initialized)
    init();

  while (1) {
    uint32_t t0 = HAL_GetTick();
    SysWatchdog_Tick();
    TosApi_Tick();
    SysUI::loop();
    uint32_t elapsed = HAL_GetTick() - t0;
    SysUI::updateCpuUsage(elapsed);
    TosApi_Tick();
    SysWatchdog_Tick();
    JPDelay(10);
  }
}

TOS::~TOS() {}

extern "C" void Tos_Start(void) {
  TOS os;
  os.init();
  os.start();
}
