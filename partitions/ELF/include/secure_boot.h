/**
 ******************************************************************************
 * @file    secure_boot.h
 * @author  Typheye
 * @brief   ELF secure boot verification interface.
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

#ifndef SECURE_BOOT_H
#define SECURE_BOOT_H

#include <stdint.h>

#include "tee_format.h"

typedef enum {
  SECUREBOOT_VERIFY_OK = 0,
  SECUREBOOT_VERIFY_BAD_ARGUMENT,
  SECUREBOOT_VERIFY_BAD_HEADER,
  SECUREBOOT_VERIFY_BAD_CRC,
  SECUREBOOT_VERIFY_ROLLBACK,
  SECUREBOOT_VERIFY_BAD_DIGEST,
  SECUREBOOT_VERIFY_BAD_SIGNATURE
} SecureBoot_Result_t;

SecureBoot_Result_t SecureBoot_Verify(uint32_t storage_address,
                                    uint32_t expected_load_address,
                                    uint32_t expected_size,
                                    uint32_t expected_type,
                                    uint32_t minimum_version);

/* Verify header, CRC, version, and SHA-256 digest only — no ECDSA.
 * Fast path (~0.1 s) for development/unlocked mode. */
SecureBoot_Result_t SecureBoot_VerifyHeaderAndHash(uint32_t storage_address,
                                    uint32_t expected_load_address,
                                    uint32_t expected_size,
                                    uint32_t expected_type,
                                    uint32_t minimum_version);

#endif /* SECURE_BOOT_H */
