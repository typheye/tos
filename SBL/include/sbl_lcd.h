#ifndef SBL_LCD_IF_H
#define SBL_LCD_IF_H

#include "sbl_common.h"

SBL_CODE void SBL_LcdInit(void);
SBL_CODE void SBL_LcdRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          uint16_t color);
SBL_CODE void SBL_LcdDrawText(uint16_t x, uint16_t y, const char *text,
                              uint16_t color, uint8_t scale);
SBL_CODE void SBL_LcdDrawTitleText(uint16_t x, uint16_t y, const char *text,
                                   uint16_t color);

#endif /* SBL_LCD_IF_H */
