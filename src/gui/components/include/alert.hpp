/**
 ******************************************************************************
 * @file    alert.hpp
 * @author  Typheye
 * @brief   Alert interface.
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

#ifndef ALERT_HPP
#define ALERT_HPP

// Show a blocking alert popup. User presses ENTER to dismiss.
void alert_show(const char *title, const char *msg);

#endif
