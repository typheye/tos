#ifndef __FONTS_H
#define __FONTS_H

#include <stdint.h>

// 字体结构体
typedef struct _pFont {
  const uint8_t *pTable; // 字模数组地址
  uint16_t Width;        // 单个字符的字模宽度
  uint16_t Height;       // 单个字符的字模长度
  uint16_t Bytes;        // 单个字符的字模数据个数
  uint16_t Count;        // 汉字字模用到，表示二维数组的行大小
} pFont, *pFONT;

// ASCII字体
extern pFONT ASCII_Font32;
extern pFONT ASCII_Font24;
extern pFONT ASCII_Font20;
extern pFONT ASCII_Font16;
extern pFONT ASCII_Font12;

#endif