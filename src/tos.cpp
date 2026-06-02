/**
 * @file    tos.cpp
 * @brief   TOS entry point — imports init from core/init.cpp, runs main loop
 */

#include "core/include/tos.hpp"
#include "include/sysui.hpp"
#include "stm32f4xx_hal.h"

TOS::TOS() : initialized_(false), tick_count_(0) {}

void TOS::start() {
  if (!initialized_)
    init();

  // 主循环
  while (1) {
    uint32_t t0 = HAL_GetTick();
    SysUI::loop();
    uint32_t elapsed = HAL_GetTick() - t0;
    SysUI::updateCpuUsage(elapsed);
    HAL_Delay(10); // 10ms 轮询间隔
  }
}

TOS::~TOS() {}

extern "C" void start_tos(void) {
  TOS os;
  os.init();
  os.start();
}
