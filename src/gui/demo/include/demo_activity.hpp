/**
 ******************************************************************************
 * @file    demo_activity.hpp
 * @author  Typheye
 * @brief   Demo Activity interface.
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

#ifndef DEMO_ACTIVITY_HPP
#define DEMO_ACTIVITY_HPP

int demo_activity_run(void); // TOS menu, returns 1 if user chose Return
void demo_list_run(void);    // Full demo list (called from Debugs)

#endif
