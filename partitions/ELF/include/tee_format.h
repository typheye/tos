/**
 ******************************************************************************
 * @file    tee_format.h
 * @author  Typheye
 * @brief   TEE state record format and validation interface.
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

#ifndef TEE_FORMAT_H
#define TEE_FORMAT_H

#include <stdint.h>

#define TOS_TEE_MANIFEST_MAGIC   0x54454531UL /* TEE1 */
#define TOS_TEE_MANIFEST_VERSION 1UL
#define TOS_TEE_STATE_MAGIC      0x54535431UL /* TST1 */
#define TOS_TEE_STATE_VERSION    1UL

#define TOS_TXN_STATE_PENDING 0xFFFFFFFEUL
#define TOS_TXN_STATE_DONE    0xFFFFFFFCUL
#define TOS_TXN_STATE_FAILED  0xFFFFFFF8UL
#define TOS_TXN_STATE_RESTART 0xFFFFFFF0UL

#define TOS_IMAGE_HEADER_MAGIC   0x31474953UL /* SIG1 */
#define TOS_IMAGE_HEADER_VERSION 1UL
#define TOS_IMAGE_HEADER_OFFSET  0x200UL
#define TOS_IMAGE_HEADER_SIZE    0x80UL

#define TOS_IMAGE_FLAG_NONE      0U

#define TOS_IMAGE_TYPE_SBL       1U
#define TOS_IMAGE_TYPE_REC       2U
#define TOS_IMAGE_TYPE_SYSTEM    3U

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t header_version;
  uint32_t image_type;
  uint32_t load_address;
  uint32_t image_size;
  uint32_t image_version;
  uint32_t flags;
  uint8_t  image_digest[32];
  uint8_t  signature[64];
  uint32_t header_crc32;
} TosImageHeader;

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t version;
  uint32_t partition_count;
  uint32_t flags;
  struct { char name[12]; uint32_t offset; uint32_t size; uint32_t attr; uint32_t crc; } partitions[8];
  uint8_t  public_key_hash[32];
  uint32_t reserved[16];
} TosTeeManifest;

typedef struct __attribute__((packed, aligned(4))) {
  uint32_t magic;
  uint32_t version;
  uint32_t sequence;
  uint32_t unlocked;
  uint32_t boot_target;
  uint32_t update_kind;
  uint32_t txn_state;
  uint32_t source_address;
  uint32_t target_address;
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t post_boot_target;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t record_crc;
} TosTeeStateRecord;

static inline uint32_t TosTeeStateRecordCrc(const TosTeeStateRecord *r) {
  return r->magic ^ r->version ^ r->sequence ^ r->unlocked ^
         r->boot_target ^ r->update_kind ^ r->txn_state ^
         r->source_address ^ r->target_address ^ r->image_size ^
         r->image_crc32 ^ r->post_boot_target ^ r->reserved0 ^
         r->reserved1 ^ r->reserved2 ^ 0xA5C35A3CUL;
}

static inline uint8_t TosTeeStateRecordValid(const TosTeeStateRecord *r) {
  if (!r || r->magic != TOS_TEE_STATE_MAGIC ||
      r->version != TOS_TEE_STATE_VERSION) return 0U;
  if (r->txn_state != 0xFFFFFFFFUL &&
      r->txn_state != TOS_TXN_STATE_PENDING &&
      r->txn_state != TOS_TXN_STATE_DONE &&
      r->txn_state != TOS_TXN_STATE_FAILED &&
      r->txn_state != TOS_TXN_STATE_RESTART) return 0U;
  return r->record_crc == TosTeeStateRecordCrc(r) ? 1U : 0U;
}

#endif /* TEE_FORMAT_H */
