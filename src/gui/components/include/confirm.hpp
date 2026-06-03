/**
 ******************************************************************************
 * @file    confirm.hpp
 * @author  Typheye
 * @brief   Confirm interface.
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

#ifndef CONFIRM_HPP
#define CONFIRM_HPP

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Show a Yes/No confirmation dialog
 * @param  title  Dialog title
 * @param  msg    Message text
 * @return true if Yes selected, false if No selected
 */
bool confirm_show(const char *title, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
