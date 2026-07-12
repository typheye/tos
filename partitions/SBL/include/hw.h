/**
 ******************************************************************************
 * @file    hw.h
 * @author  Typheye
 * @brief   SBL hardware abstraction (GPIO, UART, LED, SPI) interface.
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
#ifndef SBL_HW_H
#define SBL_HW_H

#include "common.h"
#include "manifest.h"

#define SBL_BTN_PORT GPIOA
#define SBL_BTN_PIN  15U

SBL_CODE void SBL_HwBootstrap(void);
SBL_CODE uint8_t SBL_IsFastbootRequested(void);
SBL_CODE uint8_t SBL_IsButtonDown(void);
SBL_CODE void SBL_WaitButtonRelease(uint32_t settle_ms);
SBL_CODE void SBL_LedsOff(void);
SBL_CODE void SBL_StatusLedOn(void);
SBL_CODE void SBL_StatusLedOff(void);
SBL_CODE void SBL_SystemReboot(void);
SBL_CODE void SBL_SystemRebootTo(uint32_t boot_target);

/* ── USART1 serial console (PA9 TX, PA10 RX, 115200-8N1) ──────── */
#ifndef SBL_UART_ENABLED
#define SBL_UART_ENABLED 1
#endif
#if SBL_UART_ENABLED
SBL_CODE void SBL_UartInit(void);
SBL_CODE void SBL_UartWrite(uint8_t byte);
SBL_CODE void SBL_UartWriteText(const char *text);
SBL_CODE void SBL_UartWriteLine(const char *text);
#else
#define SBL_UartInit()                                   do{}while(0)
#define SBL_UartWrite(b)                                 do{(void)(b);}while(0)
#define SBL_UartWriteText(t)                             do{(void)(t);}while(0)
#define SBL_UartWriteLine(t)                             do{(void)(t);}while(0)
#endif

#if LCD_ENABLED
SBL_CODE void SBL_Spi1InitForLcd(void);
SBL_CODE void SBL_Spi1Write(uint8_t v);
SBL_CODE void SBL_Spi1WriteBytes(const uint8_t *data, uint32_t len);
#else
#define SBL_Spi1InitForLcd()                            do{}while(0)
#define SBL_Spi1Write(v)                                do{(void)(v);}while(0)
#define SBL_Spi1WriteBytes(data,len)                     do{(void)(data);(void)(len);}while(0)
#endif

#endif /* SBL_HW_H */
