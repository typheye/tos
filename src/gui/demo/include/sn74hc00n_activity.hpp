/**
 ******************************************************************************
 * @file    sn74hc00n_activity.hpp
 * @author  Typheye
 * @brief   Sn74Hc00N Activity interface.
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

#ifndef SN74HC00N_ACTIVITY_HPP
#define SN74HC00N_ACTIVITY_HPP
#include "core/sys/include/systime.h"
#include "hardware/include/key.hpp"
#include "hardware/include/lcd.hpp"
#include "hardware/include/trtc.hpp"
#include "include/libpd.h"
#include "include/libvan.h"
#include "include/sn74hc00n.hpp"
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif


void hc00n_activity(void);


void hc00n_monitor_activity(void);


void hc00n_truth_table_activity(void);


void hc00n_test_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* SN74HC00N_ACTIVITY_HPP */