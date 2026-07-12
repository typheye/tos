/**
 ******************************************************************************
 * @file    sysfonts.h
 * @author  Typheye
 * @brief   Sysfonts interface.
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

#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>


typedef struct _pFont {
  const uint8_t *pTable;
  uint16_t Width;
  uint16_t Height;
  uint16_t Bytes;
  uint16_t Count;
} pFont, *pFONT;


extern pFONT ASCII_Font32;
extern pFONT ASCII_Font24;
extern pFONT ASCII_Font20;
extern pFONT ASCII_Font16;
extern pFONT ASCII_Font12;

#endif