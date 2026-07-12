/**
 ******************************************************************************
 * @file    tos.hpp
 * @author  Typheye
 * @brief   Tos interface.
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

#ifndef TOS_HPP
#define TOS_HPP

// ── Hardware headers ──────────────────────────────────────────
#include "include/bmp180.hpp"
#include "include/buzzer.hpp"
#include "include/esp8266.hpp"
#include "include/jy901s.hpp"
#include "include/key.hpp"
#include "include/lcd.hpp"
#include "include/led.hpp"
#include "include/lib3dgyro.h"
#include "include/libfs.h"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/pot.hpp"
#include "include/sn74hc00n.hpp"
#include "include/sysui.hpp"
#include "include/tcs3472.hpp"
#include "include/thid.hpp"
#include "include/trtc.hpp"
#include "include/tsdio.hpp"

extern "C" {
#include "include/lib3dox.h"
}

#include "main.h"
#include "stm32f4xx_hal.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "core/sdk/include/tos_api.h"
#include "core/sys/include/syswatchdog.h"

// ── Demo type ─────────────────────────────────────────────────
enum class DemoType {
  NONE = 0,
  KEY_TEST,
  SD_CARD_TEST,
  I2C_SCAN,
  JY901S_TEST,
  DISPLAY_TEST,
  GYRO_CUBE,
  BMP180_TEST
};

// ── TOS class ─────────────────────────────────────────────────
class TOS {
public:
  TOS();
  ~TOS();

  void init();
  void start();

private:
  bool _initialized;
  uint32_t _tickCount;
};

#ifdef __cplusplus
extern "C" {
#endif
void Tos_Start(void);
#ifdef __cplusplus
}
#endif

#endif // TOS_HPP
