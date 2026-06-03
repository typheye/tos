/**
 ******************************************************************************
 * @file    tos.cpp
 * @author  Typheye
 * @brief   Tos implementation.
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

#include "core/include/tos.hpp"


TOS::TOS() : initialized_(false), tick_count_(0) {}

void TOS::start() {
  if (!initialized_)
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
    HAL_Delay(10);
  }
}

TOS::~TOS() {}

extern "C" void start_tos(void) {
  TOS os;
  os.init();
  os.start();
}
