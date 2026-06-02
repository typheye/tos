/**
 * @file    tos.cpp
 * @brief   TOS entry point.
 */

#include "core/include/tos.hpp"
#include "core/sdk/include/tos_api.h"
#include "include/sysui.hpp"
#include "stm32f4xx_hal.h"
#include "core/sys/include/syswatchdog.h"

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
