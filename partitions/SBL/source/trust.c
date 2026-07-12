/**
 ******************************************************************************
 * @file    trust.c
 * @author  Typheye
 * @brief   SBL secure boot trust chain implementation.
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

#include <stdint.h>
#include "common.h"
#include "manifest.h"

#define SBL_TRUST_MAGIC   0x54525553UL /* TRUS */
#define SBL_TRUST_VERSION 2UL

typedef struct {
  char name[12];
  uint32_t offset;
  uint32_t size;
  uint32_t crc32;
} SBL_TrustPartition_t;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t partition_count;
  uint32_t reserved;
  SBL_TrustPartition_t part[4];
} SBL_TrustManifest_t;

SBL_CONST const SBL_TrustManifest_t sbl_trust_manifest = {
    SBL_TRUST_MAGIC, SBL_TRUST_VERSION, 3U, 0xFFFFFFFFUL,
    {
        {"sbl", TOS_PART_SBL_OFFSET, TOS_PART_SBL_SIZE, 0xFFFFFFFFUL},
        {"rec", TOS_PART_REC_OFFSET, TOS_PART_REC_SIZE, 0xFFFFFFFFUL},
        {"system", TOS_PART_SYSTEM_OFFSET, TOS_PART_SYSTEM_SIZE, 0xFFFFFFFFUL},
    },
};
