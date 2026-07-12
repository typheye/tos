/**
 ******************************************************************************
 * @file    image_header.c
 * @author  Typheye
 * @brief   SBL image header verification implementation.
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

#include "tee_format.h"

/* The host signing step replaces this section in exported ELF and BIN files. */
__attribute__((section(".tos_image_header"), used, aligned(4)))
const uint8_t g_tos_image_header_placeholder[TOS_IMAGE_HEADER_SIZE] = {
    [0 ... TOS_IMAGE_HEADER_SIZE - 1] = 0xFFU,
};
