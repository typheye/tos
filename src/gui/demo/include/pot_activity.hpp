/**
 ******************************************************************************
 * @file    pot_activity.hpp
 * @author  Typheye
 * @brief   Pot Activity interface.
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

#ifndef __POT_ACTIVITY_HPP
#define __POT_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// 主菜单
void pot_activity(void);

// 实时显示模式
void pot_monitor_activity(void);

// 图表显示模式 (线性图表)
void pot_chart_activity(void);

// 校准模式
void pot_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __POT_ACTIVITY_HPP */