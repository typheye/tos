/**
 ******************************************************************************
 * @file    usb.h
 * @author  Typheye
 * @brief   SBL USB CDC fastboot protocol interface.
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
