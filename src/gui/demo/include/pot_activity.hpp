/**
 ******************************************************************************
 * @file    pot_activity.hpp
 * @author  Typheye
 * @brief   Pot Activity interface.
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

#ifndef POT_ACTIVITY_HPP
#define POT_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif


void pot_activity(void);


void pot_monitor_activity(void);


void pot_chart_activity(void);


void pot_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* POT_ACTIVITY_HPP */