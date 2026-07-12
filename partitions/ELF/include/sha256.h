/**
 ******************************************************************************
 * @file    sha256.h
 * @author  Typheye
 * @brief   ELF SHA-256 hash interface.
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

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>

typedef struct {
  uint32_t state[8];
  uint64_t total_size;
  uint8_t block[64];
  uint32_t block_size;
} TosSha256Context_t;

void TosSha256Init(TosSha256Context_t *ctx);
void TosSha256Update(TosSha256Context_t *ctx, const void *data, uint32_t size);
void TosSha256Final(TosSha256Context_t *ctx, uint8_t digest[32]);

#endif /* SHA256_H */
