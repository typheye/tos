/**
 ******************************************************************************
 * @file    secure_boot.h
 * @author  Typheye
 * @brief   ELF secure boot verification interface.
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

#ifndef TOS_SECURE_BOOT_H
#define TOS_SECURE_BOOT_H

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

#endif /* TOS_SECURE_BOOT_H */
