/**
 ******************************************************************************
 * @file    sd_activity.hpp
 * @author  Typheye
 * @brief   Sd Activity interface.
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

#ifndef SD_ACTIVITY_HPP
#define SD_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif


void sd_card_activity_gui(void);
void sd_card_activity(void);
void sd_card_direct_activity(void);


void sd_card_diagnostic(
    void);
void sd_card_rw_test(void);


void sd_card_mount(void);
void sd_card_unmount(void);
void sd_card_list(void);
void sd_card_format(void);

#ifdef __cplusplus
}
#endif

#endif // SD_ACTIVITY_HPP