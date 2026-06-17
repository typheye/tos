#ifndef SBL_HW_H
#define SBL_HW_H

#include "sbl_common.h"

#define SBL_BTN_PORT GPIOA
#define SBL_BTN_PIN  15U

SBL_CODE void SBL_HwBootstrap(void);
SBL_CODE uint8_t SBL_IsFastbootRequested(void);
SBL_CODE uint8_t SBL_IsButtonDown(void);
SBL_CODE void SBL_WaitButtonRelease(uint32_t settle_ms);
SBL_CODE void SBL_LedsOff(void);
SBL_CODE void SBL_SystemReboot(void);
SBL_CODE void SBL_Spi1InitForLcd(void);
SBL_CODE void SBL_Spi1Write(uint8_t v);
SBL_CODE void SBL_Spi1WriteBytes(const uint8_t *data, uint32_t len);

#endif /* SBL_HW_H */
