/**
 ******************************************************************************
 * @file    verify.c
 * @author  Typheye
 * @brief   ELF image verification implementation.
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

#include "secure_boot.h"

#include "p256-m.h"
#include "tee_format.h"
#include "tos_secure_boot_key.h"
#include "sha256.h"

static const uint8_t root_public_key[64] = TOS_SECURE_BOOT_PUBLIC_KEY_BYTES;
static const uint8_t erased_header[TOS_IMAGE_HEADER_SIZE] = {
    [0 ... TOS_IMAGE_HEADER_SIZE - 1] = 0xFFU,
};
static const uint8_t signature_domain[16] = {
    'T', 'O', 'S', '-', 'S', 'B', '-', 'P', '2', '5', '6', '-', 'V', '1', 0, 0,
};

static uint32_t crc32(const uint8_t *data, uint32_t size) {
  uint32_t crc = 0xFFFFFFFFU;
  while (size-- != 0U) {
    crc ^= *data++;
    for (uint32_t bit = 0U; bit < 8U; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return ~crc;
}

static uint8_t equal32(const uint8_t a[32], const uint8_t b[32]) {
  uint32_t diff = 0U;
  for (uint32_t i = 0U; i < 32U; ++i) diff |= (uint32_t)(a[i] ^ b[i]);
  return diff == 0U ? 1U : 0U;
}

static void hash_image(uint32_t storage_address, uint32_t size,
                       uint8_t digest[32]) {
  TosSha256Context_t sha;
  TosSha256Init(&sha);
  TosSha256Update(&sha, (const void *)(uintptr_t)storage_address,
                  TOS_IMAGE_HEADER_OFFSET);
  TosSha256Update(&sha, erased_header, TOS_IMAGE_HEADER_SIZE);
  TosSha256Update(&sha,
                  (const void *)(uintptr_t)(storage_address +
                                            TOS_IMAGE_HEADER_OFFSET +
                                            TOS_IMAGE_HEADER_SIZE),
                  size - TOS_IMAGE_HEADER_OFFSET - TOS_IMAGE_HEADER_SIZE);
  TosSha256Final(&sha, digest);
}

static void hash_signature_input(const TosImageHeader_t *header,
                                 uint8_t digest[32]) {
  TosSha256Context_t sha;
  TosSha256Init(&sha);
  TosSha256Update(&sha, signature_domain, sizeof(signature_domain));
  TosSha256Update(&sha, header, 7U * sizeof(uint32_t));
  TosSha256Update(&sha, header->image_digest, sizeof(header->image_digest));
  TosSha256Final(&sha, digest);
}

SecureBoot_Result_t SecureBoot_Verify(uint32_t storage_address,
                                    uint32_t expected_load_address,
                                    uint32_t expected_size,
                                    uint32_t expected_type,
                                    uint32_t minimum_version) {
  const TosImageHeader_t *header;
  uint8_t image_digest[32];
  uint8_t signature_digest[32];
  if (expected_size < TOS_IMAGE_HEADER_OFFSET + TOS_IMAGE_HEADER_SIZE ||
      storage_address + expected_size < storage_address) {
    return SECUREBOOT_VERIFY_BAD_ARGUMENT;
  }
  header = (const TosImageHeader_t *)(uintptr_t)(storage_address +
                                               TOS_IMAGE_HEADER_OFFSET);
  if (header->magic != TOS_IMAGE_HEADER_MAGIC ||
      header->header_version != TOS_IMAGE_HEADER_VERSION ||
      header->image_type != expected_type ||
      header->load_address != expected_load_address ||
      header->image_size != expected_size ||
      header->flags != TOS_IMAGE_FLAG_NONE) {
    return SECUREBOOT_VERIFY_BAD_HEADER;
  }
  if (crc32((const uint8_t *)header, sizeof(*header) - sizeof(uint32_t)) !=
      header->header_crc32) {
    return SECUREBOOT_VERIFY_BAD_CRC;
  }
  if (header->image_version < minimum_version) return SECUREBOOT_VERIFY_ROLLBACK;
  hash_image(storage_address, expected_size, image_digest);
  if (!equal32(image_digest, header->image_digest)) return SECUREBOOT_VERIFY_BAD_DIGEST;
  hash_signature_input(header, signature_digest);
  if (p256_ecdsa_verify(header->signature, root_public_key,
                        signature_digest, sizeof(signature_digest)) != P256_SUCCESS) {
    return SECUREBOOT_VERIFY_BAD_SIGNATURE;
  }
  return SECUREBOOT_VERIFY_OK;
}

SecureBoot_Result_t SecureBoot_VerifyHeaderAndHash(
    uint32_t storage_address, uint32_t expected_load_address,
    uint32_t expected_size, uint32_t expected_type,
    uint32_t minimum_version) {
  const TosImageHeader_t *header;
  uint8_t image_digest[32];
  if (expected_size < TOS_IMAGE_HEADER_OFFSET + TOS_IMAGE_HEADER_SIZE ||
      storage_address + expected_size < storage_address) {
    return SECUREBOOT_VERIFY_BAD_ARGUMENT;
  }
  header = (const TosImageHeader_t *)(uintptr_t)(storage_address +
                                               TOS_IMAGE_HEADER_OFFSET);
  if (header->magic != TOS_IMAGE_HEADER_MAGIC ||
      header->header_version != TOS_IMAGE_HEADER_VERSION ||
      header->image_type != expected_type ||
      header->load_address != expected_load_address ||
      header->image_size != expected_size ||
      header->flags != TOS_IMAGE_FLAG_NONE) {
    return SECUREBOOT_VERIFY_BAD_HEADER;
  }
  if (crc32((const uint8_t *)header, sizeof(*header) - sizeof(uint32_t)) !=
      header->header_crc32) {
    return SECUREBOOT_VERIFY_BAD_CRC;
  }
  if (header->image_version < minimum_version) return SECUREBOOT_VERIFY_ROLLBACK;
  hash_image(storage_address, expected_size, image_digest);
  if (!equal32(image_digest, header->image_digest))
    return SECUREBOOT_VERIFY_BAD_DIGEST;
  return SECUREBOOT_VERIFY_OK;
}
