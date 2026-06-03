/**
 ******************************************************************************
 * @file    i2c_activity.hpp
 * @author  Typheye
 * @brief   I2C Activity interface.
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

#ifndef I2C_ACTIVITY_HPP
#define I2C_ACTIVITY_HPP

#ifdef __cplusplus
extern "C" {
#endif

void i2c_scan_activity(void);
void i2c_scan_activity_gui(void); // 新增 GUI 版本

#ifdef __cplusplus
}
#endif

#endif // I2C_ACTIVITY_HPP