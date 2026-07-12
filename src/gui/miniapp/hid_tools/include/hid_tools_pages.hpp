/**
 ******************************************************************************
 * @file    hid_tools_pages.hpp
 * @author  Typheye
 * @brief   Hid Tools Pages interface.
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

#ifndef HID_TOOLS_PAGES_HPP
#define HID_TOOLS_PAGES_HPP

#ifdef __cplusplus
extern "C" {
#endif

void hid_tools_status_page(void);
void hid_tools_vendor_page(void);
void hid_tools_quickkeys_page(void);
void hid_tools_mouse_page(void);
void hid_tools_gyro_mouse_page(void);

#ifdef __cplusplus
}
#endif

#endif /* HID_TOOLS_PAGES_HPP */
