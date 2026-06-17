#ifndef SBL_STATE_H
#define SBL_STATE_H

#include "sbl_common.h"

SBL_CODE uint8_t SBL_StateUnlocked(void);
SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked);
SBL_CODE uint8_t SBL_StateSetBootTarget(uint32_t target);
SBL_CODE uint32_t SBL_StateConsumeBootTarget(void);

#define SBL_BOOT_TARGET_NONE     0xFFFFFFFFUL
#define SBL_BOOT_TARGET_FASTBOOT 0x46424F54UL /* FBOT */
#define SBL_BOOT_TARGET_RECOVERY 0x52454356UL /* RECV */
#define SBL_BOOT_TARGET_RECOVERY_FORMAT 0x52464D54UL /* RFMT */
#define SBL_BOOT_TARGET_RECOVERY_UPGRADE 0x52555047UL /* RUPG */

#endif /* SBL_STATE_H */
