/**
 ******************************************************************************
 * @file    state.h
 * @author  Typheye
 * @brief   SBL TEE state ring interface.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
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
