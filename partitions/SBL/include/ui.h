#ifndef SBL_UI_H
#define SBL_UI_H

#include "common.h"
#include "secure_boot.h"

SBL_CODE void SBL_UiDrawFastboot(void);
SBL_CODE void SBL_UiRunFastboot(void);
SBL_CODE void SBL_UiRunSystemDamage(SecureBoot_Result_t reason);
SBL_CODE void SBL_UiRunRecoveryException(void);

#endif /* SBL_UI_H */
