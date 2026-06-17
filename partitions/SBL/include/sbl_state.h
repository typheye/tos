#ifndef SBL_STATE_H
#define SBL_STATE_H

#include "sbl_common.h"
#include "tos_partitions.h"

SBL_CODE uint8_t SBL_StateUnlocked(void);
SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked);
SBL_CODE uint8_t SBL_StateSetBootTarget(uint32_t target);
SBL_CODE uint32_t SBL_StatePeekBootTarget(void);
SBL_CODE uint32_t SBL_StateConsumeBootTarget(void);
SBL_CODE uint8_t SBL_StateScheduleUpdate(uint32_t update_kind,
                                         uint32_t target_address,
                                         uint32_t image_size,
                                         uint32_t image_crc32);

#define SBL_BOOT_TARGET_NONE             TOS_BOOT_TARGET_NONE
#define SBL_BOOT_TARGET_FASTBOOT         TOS_BOOT_TARGET_FASTBOOT
#define SBL_BOOT_TARGET_RECOVERY         TOS_BOOT_TARGET_RECOVERY
#define SBL_BOOT_TARGET_RECOVERY_FORMAT  TOS_BOOT_TARGET_RECOVERY_FORMAT
#define SBL_BOOT_TARGET_RECOVERY_UPGRADE TOS_BOOT_TARGET_RECOVERY_UPGRADE
#define SBL_BOOT_TARGET_RECOVERY_INIT    TOS_BOOT_TARGET_RECOVERY_INIT

#endif /* SBL_STATE_H */
