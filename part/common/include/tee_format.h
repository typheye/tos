#ifndef TEE_FORMAT_H
#define TEE_FORMAT_H

#include <stdint.h>
#include "tos_partitions.h"

#define TOS_TEE_MANIFEST_MAGIC   0x54454531UL /* TEE1 */
#define TOS_TEE_MANIFEST_VERSION 1UL
#define TOS_TEE_STATE_MAGIC      0x54535431UL /* TST1 */
#define TOS_TEE_STATE_VERSION    1UL

typedef struct __attribute__((packed)) {
  char name[12];
  uint32_t offset;
  uint32_t size;
  uint32_t flags;
  uint32_t crc32;
} TosTeePartition;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint32_t version;
  uint32_t partition_count;
  uint32_t flags;
  TosTeePartition partitions[8];
  uint8_t public_key_hash[32];
  uint8_t reserved[64];
} TosTeeManifest;

/* Exactly 64 bytes. txn_state is intentionally excluded from record_crc so
 * immutable ELF may change PENDING -> DONE/FAILED using only 1-to-0 writes. */
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
         r->boot_target ^ r->update_kind ^ r->source_address ^
         r->target_address ^ r->image_size ^ r->image_crc32 ^
         r->post_boot_target ^ r->reserved0 ^ r->reserved1 ^ r->reserved2 ^
         0xA5C35A3CUL;
}

static inline uint8_t TosTeeStateRecordValid(const TosTeeStateRecord *r) {
  if (!r || r->magic != TOS_TEE_STATE_MAGIC ||
      r->version != TOS_TEE_STATE_VERSION) {
    return 0U;
  }
  if (r->txn_state != 0xFFFFFFFFUL &&
      r->txn_state != TOS_TXN_STATE_PENDING &&
      r->txn_state != TOS_TXN_STATE_DONE &&
      r->txn_state != TOS_TXN_STATE_FAILED) {
    return 0U;
  }
  return r->record_crc == TosTeeStateRecordCrc(r) ? 1U : 0U;
}

#endif /* TEE_FORMAT_H */
