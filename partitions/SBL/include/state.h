/**
 ******************************************************************************
 * @file    state.h
 * @author  Typheye
 * @brief   SBL TEE state ring interface.
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
#ifndef SBL_STATE_H
#define SBL_STATE_H

#include "common.h"
#include "manifest.h"

SBL_CODE uint8_t SBL_StateUnlocked(void);
SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked);
SBL_CODE uint8_t SBL_StateSetBootTarget(uint32_t target);
SBL_CODE uint32_t SBL_StatePeekBootTarget(void);
SBL_CODE uint32_t SBL_StateConsumeBootTarget(void);
SBL_CODE uint8_t SBL_StateScheduleUpdate(uint32_t update_kind,
                                         uint32_t target_address,
                                         uint32_t image_size,
                                         uint32_t image_crc32);
SBL_CODE uint8_t SBL_StateScheduleUpdatePost(uint32_t update_kind,
                                             uint32_t target_address,
                                             uint32_t image_size,
                                             uint32_t image_crc32,
                                             uint32_t post_boot_target);

#define SBL_BOOT_TARGET_NONE             TOS_BOOT_TARGET_NONE
#define SBL_BOOT_TARGET_FASTBOOT         TOS_BOOT_TARGET_FASTBOOT
#define SBL_BOOT_TARGET_RECOVERY         TOS_BOOT_TARGET_RECOVERY
#define SBL_BOOT_TARGET_RECOVERY_FORMAT  TOS_BOOT_TARGET_RECOVERY_FORMAT
#define SBL_BOOT_TARGET_RECOVERY_UPGRADE TOS_BOOT_TARGET_RECOVERY_UPGRADE
#define SBL_BOOT_TARGET_RECOVERY_INIT    TOS_BOOT_TARGET_RECOVERY_INIT

SBL_CODE uint8_t SBL_StateSetRestart(uint32_t boot_target);

#endif /* SBL_STATE_H */
