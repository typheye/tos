#include "sbl_flash.h"

#include <stddef.h>

#include "sbl_state.h"
#include "tos_partitions.h"

typedef struct {
  uint32_t start;
  uint32_t size;
  uint32_t index;
} SBL_SectorInfo;

static const char part_sbl[] SBL_CONST = "sbl";
static const char part_rec[] SBL_CONST = "rec";
static const char part_sah[] SBL_CONST = "sah";
static const char part_system[] SBL_CONST = "system";

static const SBL_FlashPartition sbl_partitions[] SBL_CONST = {
    {part_sbl, TOS_PART_SBL_OFFSET, TOS_PART_SBL_ADDRESS,
     TOS_TMP_STAGE_ADDRESS, TOS_PART_SBL_SIZE, 1U, 1U, 0U, 0U},
    {part_rec, TOS_PART_REC_OFFSET, TOS_PART_REC_ADDRESS,
     TOS_TMP_STAGE_ADDRESS, TOS_PART_REC_SIZE, 1U, 1U, 0U, 0U},
    {part_sah, TOS_PART_SAH_OFFSET, TOS_PART_SAH_ADDRESS,
     TOS_PART_SAH_ADDRESS, TOS_PART_SAH_SIZE, 0U, 1U, 1U, 0U},
    {part_system, TOS_PART_SYSTEM_OFFSET, TOS_PART_SYSTEM_ADDRESS,
     TOS_PART_SYSTEM_ADDRESS, TOS_PART_SYSTEM_SIZE, 0U, 1U, 1U, 0U},
};

static const SBL_SectorInfo sectors[] SBL_CONST = {
    {0x08000000UL, 0x4000UL, 0U}, {0x08004000UL, 0x4000UL, 1U},
    {0x08008000UL, 0x4000UL, 2U}, {0x0800C000UL, 0x4000UL, 3U},
    {0x08010000UL, 0x10000UL, 4U}, {0x08020000UL, 0x20000UL, 5U},
    {0x08040000UL, 0x20000UL, 6U}, {0x08060000UL, 0x20000UL, 7U},
    {0x08080000UL, 0x20000UL, 8U}, {0x080A0000UL, 0x20000UL, 9U},
    {0x080C0000UL, 0x20000UL, 10U}, {0x080E0000UL, 0x20000UL, 11U},
};

static SBL_CODE uint8_t ascii_equal(const char *a, const char *b) {
  uint32_t i = 0U;
  if (!a || !b) return 0U;
  while (a[i] && b[i]) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + ('a' - 'A'));
    if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + ('a' - 'A'));
    if (ca != cb) return 0U;
    ++i;
  }
  return a[i] == 0 && b[i] == 0 ? 1U : 0U;
}

static SBL_CODE const SBL_SectorInfo *sector_at(uint32_t address) {
  for (uint32_t i = 0U; i < sizeof(sectors) / sizeof(sectors[0]); ++i) {
    if (sectors[i].start == address) return &sectors[i];
  }
  return NULL;
}

static SBL_CODE uint8_t erase_range(uint32_t address, uint32_t size) {
  uint32_t end = address + size;
  if (end < address || !SBL_FlashUnlock()) return 0U;
  while (address < end) {
    const SBL_SectorInfo *s = sector_at(address);
    if (!s || address + s->size > end || !SBL_FlashEraseSectorIndex(s->index)) {
      SBL_FlashLock();
      return 0U;
    }
    address += s->size;
  }
  SBL_FlashLock();
  return 1U;
}

static SBL_CODE uint8_t program_words(uint32_t address,
                                       const uint8_t *data,
                                       uint32_t size) {
  if (!data || (address & 3U) != 0U || (size & 3U) != 0U ||
      !SBL_FlashUnlock()) return 0U;
  for (uint32_t off = 0U; off < size; off += 4U) {
    uint32_t word = (uint32_t)data[off] |
                    ((uint32_t)data[off + 1U] << 8) |
                    ((uint32_t)data[off + 2U] << 16) |
                    ((uint32_t)data[off + 3U] << 24);
    if (!SBL_FlashProgramWord(address + off, word)) {
      SBL_FlashLock();
      return 0U;
    }
  }
  SBL_FlashLock();
  return 1U;
}

SBL_CODE const SBL_FlashPartition *SBL_FlashFindPartition(const char *name) {
  for (uint32_t i = 0U; i < sizeof(sbl_partitions) / sizeof(sbl_partitions[0]); ++i) {
    if (ascii_equal(name, sbl_partitions[i].name)) return &sbl_partitions[i];
  }
  return NULL;
}

SBL_CODE uint32_t SBL_FlashChunkSize(void) { return SBL_FLASH_CHUNK_SIZE; }
SBL_CODE uint32_t SBL_FlashCrc32Seed(void) { return 0xFFFFFFFFUL; }

SBL_CODE uint32_t SBL_FlashCrc32Update(uint32_t crc, const uint8_t *data,
                                       uint32_t len) {
  while (len-- != 0U) {
    crc ^= *data++;
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
  }
  return crc;
}

SBL_CODE uint32_t SBL_FlashCrc32Finish(uint32_t crc) { return ~crc; }

SBL_CODE uint8_t SBL_FlashBegin(SBL_FlashSession *session,
                                const SBL_FlashPartition *part,
                                uint32_t size, uint32_t expected_crc) {
  if (!session || !part || !part->allow_flash || size != part->size ||
      expected_crc == 0xFFFFFFFFUL) return 0U;
  if (part->staged) {
    if (part->size > TOS_TMP_STAGE_SIZE ||
        !erase_range(TOS_PART_TMP_ADDRESS, TOS_PART_TMP_SIZE)) return 0U;
  } else if (!erase_range(part->write_address, part->size)) {
    return 0U;
  }
  session->part = part;
  session->expected_size = size;
  session->expected_crc = expected_crc;
  session->received = 0U;
  session->running_crc = SBL_FlashCrc32Seed();
  session->write_address = part->write_address;
  session->active = 1U;
  session->requires_reset = 0U;
  return 1U;
}

SBL_CODE uint8_t SBL_FlashErasePartition(const SBL_FlashPartition *part) {
  if (!part || !part->allow_erase || part->staged) return 0U;
  return erase_range(part->address, part->size);
}

SBL_CODE uint8_t SBL_FlashWriteChunk(SBL_FlashSession *session,
                                     uint32_t offset, const uint8_t *data,
                                     uint32_t len, uint32_t chunk_crc) {
  uint32_t calc;
  if (!session || !session->active || !data || len == 0U ||
      (offset & 3U) != 0U || (len & 3U) != 0U ||
      offset != session->received || offset + len > session->expected_size) {
    return 0U;
  }
  calc = SBL_FlashCrc32Finish(
      SBL_FlashCrc32Update(SBL_FlashCrc32Seed(), data, len));
  if (calc != chunk_crc ||
      !program_words(session->write_address + offset, data, len)) return 0U;
  session->running_crc = SBL_FlashCrc32Update(session->running_crc, data, len);
  session->received += len;
  return 1U;
}

SBL_CODE uint8_t SBL_FlashFinalize(SBL_FlashSession *session) {
  uint32_t crc;
  uint32_t kind;
  if (!session || !session->active || !session->part ||
      session->received != session->expected_size ||
      SBL_FlashCrc32Finish(session->running_crc) != session->expected_crc) {
    SBL_FlashAbort(session);
    return 0U;
  }
  crc = SBL_FlashCrc32Finish(SBL_FlashCrc32Update(
      SBL_FlashCrc32Seed(), (const uint8_t *)session->write_address,
      session->expected_size));
  if (crc != session->expected_crc) {
    SBL_FlashAbort(session);
    return 0U;
  }
  if (session->part->staged) {
    kind = session->part->address == TOS_PART_SBL_ADDRESS ? TOS_UPDATE_SBL
                                                         : TOS_UPDATE_REC;
    if (!SBL_StateScheduleUpdate(kind, session->part->address,
                                 session->expected_size,
                                 session->expected_crc)) {
      SBL_FlashAbort(session);
      return 0U;
    }
    session->requires_reset = 1U;
  }
  session->active = 0U;
  return 1U;
}

SBL_CODE void SBL_FlashAbort(SBL_FlashSession *session) {
  if (!session) return;
  session->part = NULL;
  session->expected_size = 0U;
  session->expected_crc = 0U;
  session->received = 0U;
  session->running_crc = 0U;
  session->write_address = 0U;
  session->active = 0U;
  session->requires_reset = 0U;
}
