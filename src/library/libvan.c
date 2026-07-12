/**
 ******************************************************************************
 * @file    libvan.c
 * @author  Typheye
 * @brief   Libvan implementation.
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

#include "include/libvan.h"


#define MAX_PRECISION 6
#define MIN_PRECISION 0
#define BUFFER_SIZE 16

void float_to_str(float value, char *str) {
  int is_negative = 0;
  float abs_val = value;

  if (value < 0) {
    is_negative = 1;
    abs_val = -value;
  }

  int int_part = (int)abs_val;
  int frac_part = (int)((abs_val - int_part) * 100.0f + 0.5f);

  if (frac_part >= 100) {
    int_part++;
    frac_part -= 100;
  }

  if (is_negative) {
    sprintf(str, "-%d.%02d", int_part, frac_part);
  } else {
    sprintf(str, "%d.%02d", int_part, frac_part);
  }
}

void float_to_str_precision(float value, char *str, int precision) {
  if (precision < MIN_PRECISION)
    precision = MIN_PRECISION;
  if (precision > MAX_PRECISION)
    precision = MAX_PRECISION;

  float multiplier = 1.0f;
  for (int i = 0; i < precision; i++) {
    multiplier *= 10.0f;
  }

  int is_negative = 0;
  float abs_val = value;
  if (value < 0) {
    is_negative = 1;
    abs_val = -value;
  }

  int int_part = (int)abs_val;
  int frac_part = (int)((abs_val - int_part) * multiplier + 0.5f);

  if (frac_part >= (int)multiplier) {
    int_part++;
    frac_part -= (int)multiplier;
  }

  if (precision == 0) {
    sprintf(str, "%s%d", is_negative ? "-" : "", int_part);
  } else {
    char frac_str[BUFFER_SIZE];
    char temp[BUFFER_SIZE];

    sprintf(frac_str, "%%0%dd", precision);
    sprintf(temp, frac_str, frac_part);
    sprintf(str, "%s%d.%s", is_negative ? "-" : "", int_part, temp);
  }
}

void float_to_str_signed(float value, char *str) {
  float_to_str(value, str);
}
