/**
 ******************************************************************************
 * @file    hid_tools_pages.hpp
 * @author  Typheye
 * @brief   Hid Tools Pages interface.
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
