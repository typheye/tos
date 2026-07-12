/**
 ******************************************************************************
 * @file    lcd.h
 * @author  Typheye
 * @brief   SBL LCD driver interface.
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
#ifndef SBL_LCD_IF_H
#define SBL_LCD_IF_H

#include "common.h"
#include "manifest.h"

#if LCD_ENABLED

SBL_CODE void SBL_LcdInit(void);
SBL_CODE void SBL_LcdInitDark(void);
SBL_CODE void SBL_LcdBacklightOff(void);
SBL_CODE void SBL_LcdBacklightFull(void);
SBL_CODE void SBL_LcdDisplayOff(void);
SBL_CODE void SBL_LcdDisplayOn(void);
SBL_CODE void SBL_LcdRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint16_t color);
SBL_CODE void SBL_LcdDrawText(uint16_t x, uint16_t y, const char *text,
                              uint16_t color, uint8_t scale);
SBL_CODE void SBL_LcdDrawTitleText(uint16_t x, uint16_t y, const char *text,
                                   uint16_t color);

#else

#define SBL_LcdInit()            do{}while(0)
#define SBL_LcdInitDark()        do{}while(0)
#define SBL_LcdBacklightOff()    do{}while(0)
#define SBL_LcdBacklightFull()   do{}while(0)
#define SBL_LcdDisplayOff()      do{}while(0)
#define SBL_LcdDisplayOn()       do{}while(0)
#define SBL_LcdRect(x,y,w,h,c)   do{(void)(x);(void)(y);(void)(w);(void)(h);(void)(c);}while(0)
#define SBL_LcdDrawText(x,y,t,c,s) do{(void)(x);(void)(y);(void)(t);(void)(c);(void)(s);}while(0)
#define SBL_LcdDrawTitleText(x,y,t,c) do{(void)(x);(void)(y);(void)(t);(void)(c);}while(0)

#endif /* LCD_ENABLED */
#endif /* SBL_LCD_IF_H */
