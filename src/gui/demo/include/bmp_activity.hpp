/**
 ******************************************************************************
 * @file    bmp_activity.hpp
 * @author  Typheye
 * @brief   Bmp Activity interface.
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

#ifndef __BMP_ACTIVITY_HPP
#define __BMP_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

// BMP180 GUI 主菜单
void bmp180_activity(void);

// 实时显示模式（温度/气压/海拔）
void bmp180_display_activity(void);

// 图表模式（气压/温度曲线）
void bmp180_chart_activity(void);

// 校准设置
void bmp180_calibrate_activity(void);

#ifdef __cplusplus
}
#endif

#endif /* __BMP_ACTIVITY_HPP */