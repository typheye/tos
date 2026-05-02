/**
 ******************************************************************************
 * @file    libvan.h
 * @author  Binchao Hu, Yutao Cheng
 * @brief   Utility library for value-to-value conversions
 *          This file provides functions for converting floating point values
 *          to strings with various formatting options.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 JanPNP Development Team. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

#ifndef LIBVAN_H
#define LIBVAN_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

void float_to_str(float value, char *str);
void float_to_str_precision(float value, char *str, int precision);
void float_to_str_signed(float value, char *str);

#ifdef __cplusplus
}
#endif

#endif /* LIBVAN_H */
