#ifndef SBL_USB_H
#define SBL_USB_H

#include "sbl_common.h"
#include <stdint.h>

SBL_CODE uint8_t SBL_USB_Init(void);
SBL_CODE void SBL_USB_DeInit(void);
SBL_CODE void SBL_USB_Tick(void);
SBL_CODE uint8_t SBL_USB_IsConfigured(void);
SBL_CODE uint8_t SBL_USB_Write(const uint8_t *data, uint16_t len);
SBL_CODE uint8_t SBL_USB_WriteText(const char *text);
SBL_CODE uint8_t SBL_USB_WriteTextWait(const char *text);
SBL_CODE uint8_t SBL_USB_ConsumeUnlockRequest(void);
SBL_CODE uint8_t SBL_USB_ConsumeBootloaderReloadRequest(void);
SBL_CODE void SBL_USB_SendUnlockResult(uint8_t accepted, uint8_t flash_ok);
SBL_CODE uint8_t SBL_USB_IsBusy(void);

#endif /* SBL_USB_H */
