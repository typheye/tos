/**
 ******************************************************************************
 * @file    usb.h
 * @author  Typheye
 * @brief   SBL USB CDC fastboot protocol interface.
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
#ifndef SBL_USB_H
#define SBL_USB_H

#include "common.h"
#include <stdint.h>

SBL_CODE uint8_t SBL_USB_Init(void);
SBL_CODE void SBL_USB_DeInit(void);
SBL_CODE void SBL_USB_DisconnectPulse(void);
SBL_CODE void SBL_USB_Tick(void);
SBL_CODE uint8_t SBL_USB_IsConfigured(void);
SBL_CODE uint8_t SBL_USB_Write(const uint8_t *data, uint16_t len);
SBL_CODE uint8_t SBL_USB_WriteText(const char *text);
SBL_CODE uint8_t SBL_USB_WriteTextWait(const char *text);
SBL_CODE uint8_t SBL_USB_IsBusy(void);

#endif /* SBL_USB_H */
