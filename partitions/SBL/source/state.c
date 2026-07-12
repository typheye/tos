/**
 ******************************************************************************
 * @file    state.c
 * @author  Typheye
 * @brief   SBL TEE state ring read/write implementation.
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
#include "state.h"

#include <stddef.h>
#include <stdint.h>

#include "tee_format.h"

static SBL_CODE uint8_t sbl_state_erased(const TosTeeStateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  for (uint32_t i = 0U; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (w[i] != 0xFFFFFFFFUL) return 0U;
  }
  return 1U;
}

static SBL_CODE const TosTeeStateRecord *sbl_state_latest(void) {
  const TosTeeStateRecord *latest = NULL;
  for (uint32_t off = 0U; off + sizeof(TosTeeStateRecord) <= TOS_TEE_STATE_SIZE;
       off += sizeof(TosTeeStateRecord)) {
    const TosTeeStateRecord *r =
        (const TosTeeStateRecord *)(TOS_TEE_STATE_ADDRESS + off);
    if (sbl_state_erased(r)) break;
    if (TosTeeStateRecordValid(r) && (!latest || r->sequence >= latest->sequence)) {
      latest = r;
    }
  }
  return latest;
}

static SBL_CODE uint32_t sbl_state_free_address(void) {
  for (uint32_t off = 0U; off + sizeof(TosTeeStateRecord) <= TOS_TEE_STATE_SIZE;
       off += sizeof(TosTeeStateRecord)) {
    const TosTeeStateRecord *r =
        (const TosTeeStateRecord *)(TOS_TEE_STATE_ADDRESS + off);
    if (sbl_state_erased(r)) return TOS_TEE_STATE_ADDRESS + off;
  }
  return 0U;
}

static SBL_CODE uint8_t sbl_state_program(uint32_t address,
                                           const TosTeeStateRecord *r) {
  const uint32_t *w = (const uint32_t *)r;
  if (!address || !r || !SBL_FlashUnlock()) return 0U;
  for (uint32_t i = 0U; i < sizeof(*r) / sizeof(uint32_t); ++i) {
    if (!SBL_FlashProgramWord(address + i * 4U, w[i])) {
      SBL_FlashLock();
      return 0U;
    }
  }
  SBL_FlashLock();
  return TosTeeStateRecordValid((const TosTeeStateRecord *)address);
}

static SBL_CODE uint8_t sbl_state_append(uint8_t unlocked,
                                          uint32_t boot_target,
                                          uint32_t update_kind,
                                          uint32_t txn_state,
                                          uint32_t target_address,
                                          uint32_t image_size,
                                          uint32_t image_crc32,
                                          uint32_t post_boot_target) {
  TosTeeStateRecord r;
  const TosTeeStateRecord *latest = sbl_state_latest();
  uint32_t address = sbl_state_free_address();
  if (!address) return 0U;

  r.magic = TOS_TEE_STATE_MAGIC;
  r.version = TOS_TEE_STATE_VERSION;
  r.sequence = latest ? latest->sequence + 1U : 1U;
  r.unlocked = unlocked ? 1U : 0U;
  r.boot_target = boot_target;
  r.update_kind = update_kind;
  r.txn_state = txn_state;
  r.source_address = update_kind == TOS_UPDATE_NONE ? 0xFFFFFFFFUL
                                                    : TOS_TMP_STAGE_ADDRESS;
  r.target_address = target_address;
  r.image_size = image_size;
  r.image_crc32 = image_crc32;
  r.post_boot_target = post_boot_target;
  r.reserved0 = 0xFFFFFFFFUL;
  r.reserved1 = 0xFFFFFFFFUL;
  r.reserved2 = 0xFFFFFFFFUL;
  r.record_crc = TosTeeStateRecordCrc(&r);
  return sbl_state_program(address, &r);
}

SBL_CODE uint8_t SBL_StateUnlocked(void) {
  const TosTeeStateRecord *r = sbl_state_latest();
  return r && r->unlocked ? 1U : 0U;
}

SBL_CODE uint8_t SBL_StateSetRestart(uint32_t boot_target) {
  return sbl_state_append(SBL_StateUnlocked(), boot_target,
                          TOS_UPDATE_NONE, TOS_TXN_STATE_RESTART,
                          0xFFFFFFFFUL, 0U, 0xFFFFFFFFUL,
                          SBL_BOOT_TARGET_NONE);
}

SBL_CODE uint8_t SBL_StateSetUnlocked(uint8_t unlocked) {
  /* Locking or unlocking the bootloader must force a data wipe before the
   * normal boot path resumes. REC may currently compile the wipe as a
   * development fake, but the RECOVERY_FORMAT transition itself is intentional
   * security behavior and must not be removed as a cosmetic side effect. */
  return sbl_state_append(unlocked, SBL_BOOT_TARGET_RECOVERY_FORMAT,
                          TOS_UPDATE_NONE, 0xFFFFFFFFUL, 0xFFFFFFFFUL,
                          0U, 0xFFFFFFFFUL, SBL_BOOT_TARGET_NONE);
}

SBL_CODE uint8_t SBL_StateSetBootTarget(uint32_t target) {
  return sbl_state_append(SBL_StateUnlocked(), target, TOS_UPDATE_NONE,
                          0xFFFFFFFFUL, 0xFFFFFFFFUL, 0U,
                          0xFFFFFFFFUL, SBL_BOOT_TARGET_NONE);
}

SBL_CODE uint8_t SBL_StateScheduleUpdate(uint32_t update_kind,
                                         uint32_t target_address,
                                         uint32_t image_size,
                                         uint32_t image_crc32) {
  return SBL_StateScheduleUpdatePost(update_kind, target_address, image_size,
                                     image_crc32, SBL_BOOT_TARGET_FASTBOOT);
}

SBL_CODE uint8_t SBL_StateScheduleUpdatePost(uint32_t update_kind,
                                             uint32_t target_address,
                                             uint32_t image_size,
                                             uint32_t image_crc32,
                                             uint32_t post_boot_target) {
  if ((update_kind != TOS_UPDATE_SBL && update_kind != TOS_UPDATE_REC) ||
      image_crc32 == 0xFFFFFFFFUL) {
    return 0U;
  }
  return sbl_state_append(SBL_StateUnlocked(), post_boot_target,
                          update_kind, TOS_TXN_STATE_PENDING,
                          target_address, image_size, image_crc32,
                          post_boot_target);
}

SBL_CODE uint32_t SBL_StatePeekBootTarget(void) {
  const TosTeeStateRecord *r = sbl_state_latest();
  if (!r || r->txn_state == TOS_TXN_STATE_PENDING) return SBL_BOOT_TARGET_NONE;
  if (r->boot_target == SBL_BOOT_TARGET_FASTBOOT ||
      r->boot_target == SBL_BOOT_TARGET_RECOVERY ||
      r->boot_target == SBL_BOOT_TARGET_RECOVERY_FORMAT ||
      r->boot_target == SBL_BOOT_TARGET_RECOVERY_UPGRADE ||
      r->boot_target == SBL_BOOT_TARGET_RECOVERY_INIT) {
    return r->boot_target;
  }
  return SBL_BOOT_TARGET_NONE;
}

SBL_CODE uint32_t SBL_StateConsumeBootTarget(void) {
  const TosTeeStateRecord *r = sbl_state_latest();
  uint32_t target;
  if (!r || r->txn_state == TOS_TXN_STATE_PENDING) return SBL_BOOT_TARGET_NONE;
  target = r->boot_target;
  if (target != SBL_BOOT_TARGET_FASTBOOT &&
      target != SBL_BOOT_TARGET_RECOVERY &&
      target != SBL_BOOT_TARGET_RECOVERY_FORMAT &&
      target != SBL_BOOT_TARGET_RECOVERY_UPGRADE &&
      target != SBL_BOOT_TARGET_RECOVERY_INIT) {
    return SBL_BOOT_TARGET_NONE;
  }
  if (!sbl_state_append(r->unlocked ? 1U : 0U, SBL_BOOT_TARGET_NONE,
                        TOS_UPDATE_NONE, 0xFFFFFFFFUL, 0xFFFFFFFFUL,
                        0U, 0xFFFFFFFFUL, SBL_BOOT_TARGET_NONE)) {
    return SBL_BOOT_TARGET_NONE;
  }
  return target;
}
