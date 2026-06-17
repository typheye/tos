#include "sbl_flash.h"

#include "main.h"
#include "sbl_mem.h"

#define SBL_FLASH_BASE        0x08000000UL
#define SBL_FLASH_TOTAL_SIZE  0x00100000UL
#define SBL_FLASH_CHUNK_SIZE  (16UL * 1024UL)

#define SBL_PART_SBL_OFFSET    0x00000000UL
#define SBL_PART_SBL_SIZE      0x00010000UL
#define SBL_PART_SRE_OFFSET    0x00010000UL
#define SBL_PART_SRE_SIZE      0x00010000UL
#define SBL_PART_SAH_OFFSET    0x00020000UL
#define SBL_PART_SAH_SIZE      0x00020000UL
#define SBL_PART_SYSTEM_OFFSET 0x00040000UL
#define SBL_PART_SYSTEM_SIZE   0x00080000UL

typedef struct {
  uint32_t start;
  uint32_t size;
  uint32_t sector_index;
} SBL_SectorInfo;

static const char sbl_part_name_sbl[] SBL_CONST = "sbl";
static const char sbl_part_name_sre[] SBL_CONST = "sre";
static const char sbl_part_name_sah[] SBL_CONST = "sah";
static const char sbl_part_name_system[] SBL_CONST = "system";

static const SBL_FlashPartition sbl_partitions[] SBL_CONST = {
    {sbl_part_name_sbl,    SBL_PART_SBL_OFFSET,    SBL_FLASH_BASE + SBL_PART_SBL_OFFSET,    SBL_PART_SBL_SIZE,    1U, 1U, 0U, 0U},
    {sbl_part_name_sre,    SBL_PART_SRE_OFFSET,    SBL_FLASH_BASE + SBL_PART_SRE_OFFSET,    SBL_PART_SRE_SIZE,    0U, 0U, 1U, 1U},
    {sbl_part_name_sah,    SBL_PART_SAH_OFFSET,    SBL_FLASH_BASE + SBL_PART_SAH_OFFSET,    SBL_PART_SAH_SIZE,    0U, 0U, 1U, 1U},
    {sbl_part_name_system, SBL_PART_SYSTEM_OFFSET, SBL_FLASH_BASE + SBL_PART_SYSTEM_OFFSET, SBL_PART_SYSTEM_SIZE, 0U, 0U, 1U, 1U},
};

static const SBL_SectorInfo sbl_sectors[] SBL_CONST = {
    {0x08000000UL,  16UL * 1024UL, 0U},
    {0x08004000UL,  16UL * 1024UL, 1U},
    {0x08008000UL,  16UL * 1024UL, 2U},
    {0x0800C000UL,  16UL * 1024UL, 3U},
    {0x08010000UL,  64UL * 1024UL, 4U},
    {0x08020000UL, 128UL * 1024UL, 5U},
    {0x08040000UL, 128UL * 1024UL, 6U},
    {0x08060000UL, 128UL * 1024UL, 7U},
    {0x08080000UL, 128UL * 1024UL, 8U},
    {0x080A0000UL, 128UL * 1024UL, 9U},
    {0x080C0000UL, 128UL * 1024UL, 10U},
    {0x080E0000UL, 128UL * 1024UL, 11U},
};

static SBL_CODE uint8_t sbl_streq_ascii(const char *a, const char *b) {
  uint32_t i = 0U;
  while (a && b && a[i] && b[i]) {
    char ca = a[i];
    char cb = b[i];
    if (ca >= 'A' && ca <= 'Z') {
      ca = (char)(ca + ('a' - 'A'));
    }
    if (cb >= 'A' && cb <= 'Z') {
      cb = (char)(cb + ('a' - 'A'));
    }
    if (ca != cb) {
      return 0U;
    }
    i++;
  }
  return a && b && a[i] == 0 && b[i] == 0;
}

static SBL_CODE const SBL_SectorInfo *sbl_sector_for_addr(uint32_t addr) {
  for (uint32_t i = 0U; i < (uint32_t)(sizeof(sbl_sectors) / sizeof(sbl_sectors[0])); ++i) {
    uint32_t start = sbl_sectors[i].start;
    uint32_t end = start + sbl_sectors[i].size;
    if (addr >= start && addr < end) {
      return &sbl_sectors[i];
    }
  }
  return 0;
}

static SBL_CODE uint8_t sbl_flash_program_words(uint32_t addr, const uint8_t *data, uint32_t len) {
  if ((addr & 3U) != 0U || (len & 3U) != 0U) {
    return 0U;
  }
  for (uint32_t off = 0U; off < len; off += 4U) {
    uint32_t word = ((uint32_t)data[off]) |
                    ((uint32_t)data[off + 1U] << 8) |
                    ((uint32_t)data[off + 2U] << 16) |
                    ((uint32_t)data[off + 3U] << 24);
    if (!SBL_FlashProgramWord(addr + off, word)) {
      return 0U;
    }
  }
  return 1U;
}

static SBL_CODE uint8_t sbl_flash_erase_sector(uint32_t sector_index) {
  return SBL_FlashEraseSectorIndex(sector_index);
}

static SBL_CODE void sbl_flash_release_suffix(SBL_FlashSession *session) {
  if (session->suffix_buf) {
    SBL_MemFree(session->suffix_buf);
    session->suffix_buf = 0;
  }
  session->suffix_addr = 0U;
  session->suffix_size = 0U;
}

static SBL_CODE uint8_t sbl_flash_begin_sector(SBL_FlashSession *session, uint32_t addr) {
  const SBL_SectorInfo *sector = sbl_sector_for_addr(addr);
  uint32_t part_start;
  uint32_t part_end;
  uint32_t prefix_size;
  uint8_t *prefix_buf = 0;

  if (!session || !sector || !session->part) {
    return 0U;
  }

  sbl_flash_release_suffix(session);

  part_start = session->part->address;
  part_end = session->part->address + session->expected_size;
  session->current_sector = sector->sector_index;
  session->current_sector_start = sector->start;
  session->current_sector_size = sector->size;
  session->current_range_start = (part_start > sector->start) ? part_start : sector->start;
  session->current_range_end = (part_end < (sector->start + sector->size)) ? part_end : (sector->start + sector->size);

  prefix_size = session->current_range_start - session->current_sector_start;
  if (prefix_size > 0U) {
    const uint8_t *prefix = (const uint8_t *)session->current_sector_start;
    prefix_buf = (uint8_t *)SBL_MemAlloc(prefix_size);
    if (!prefix_buf) {
      return 0U;
    }
    for (uint32_t i = 0U; i < prefix_size; ++i) {
      prefix_buf[i] = prefix[i];
    }
  }
  if (session->current_range_end < session->current_sector_start + session->current_sector_size) {
    session->suffix_addr = session->current_range_end;
    session->suffix_size = (session->current_sector_start + session->current_sector_size) - session->current_range_end;
    session->suffix_buf = (uint8_t *)SBL_MemAlloc(session->suffix_size);
    if (!session->suffix_buf) {
      if (prefix_buf) {
        SBL_MemFree(prefix_buf);
      }
      return 0U;
    }
    const uint8_t *src = (const uint8_t *)session->suffix_addr;
    for (uint32_t i = 0U; i < session->suffix_size; ++i) {
      session->suffix_buf[i] = src[i];
    }
  }

  if (!SBL_FlashUnlock()) {
    if (prefix_buf) {
      SBL_MemFree(prefix_buf);
    }
    sbl_flash_release_suffix(session);
    return 0U;
  }
  if (!sbl_flash_erase_sector(session->current_sector)) {
    SBL_FlashLock();
    if (prefix_buf) {
      SBL_MemFree(prefix_buf);
    }
    sbl_flash_release_suffix(session);
    return 0U;
  }

  if (prefix_size > 0U) {
    if (!sbl_flash_program_words(session->current_sector_start, prefix_buf, prefix_size)) {
      SBL_MemFree(prefix_buf);
      SBL_FlashLock();
      sbl_flash_release_suffix(session);
      return 0U;
    }
  }
  if (prefix_buf) {
    SBL_MemFree(prefix_buf);
  }

  SBL_FlashLock();
  return 1U;
}

static SBL_CODE uint8_t sbl_flash_finish_sector(SBL_FlashSession *session) {
  uint8_t ok = 1U;
  if (!session || !session->suffix_size) {
    sbl_flash_release_suffix(session);
    return 1U;
  }
  if (!SBL_FlashUnlock()) {
    return 0U;
  }
  ok = sbl_flash_program_words(session->suffix_addr, session->suffix_buf, session->suffix_size);
  SBL_FlashLock();
  sbl_flash_release_suffix(session);
  return ok;
}

static SBL_CODE void sbl_flash_abort_restore_suffix(SBL_FlashSession *session) {
  if (!session || !session->suffix_buf || session->suffix_size == 0U) {
    return;
  }
  if (SBL_FlashUnlock()) {
    (void)sbl_flash_program_words(session->suffix_addr, session->suffix_buf,
                                  session->suffix_size);
    SBL_FlashLock();
  }
}

SBL_CODE const SBL_FlashPartition *SBL_FlashFindPartition(const char *name) {
  for (uint32_t i = 0U; i < (uint32_t)(sizeof(sbl_partitions) / sizeof(sbl_partitions[0])); ++i) {
    if (sbl_streq_ascii(name, sbl_partitions[i].name)) {
      return &sbl_partitions[i];
    }
  }
  return 0;
}

SBL_CODE uint32_t SBL_FlashChunkSize(void) {
  return SBL_FLASH_CHUNK_SIZE;
}

SBL_CODE uint32_t SBL_FlashCrc32Seed(void) {
  return 0xFFFFFFFFUL;
}

SBL_CODE uint32_t SBL_FlashCrc32Update(uint32_t crc, const uint8_t *data, uint32_t len) {
  for (uint32_t i = 0U; i < len; ++i) {
    crc ^= (uint32_t)data[i];
    for (uint32_t bit = 0U; bit < 8U; ++bit) {
      uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }
  return crc;
}

SBL_CODE uint32_t SBL_FlashCrc32Finish(uint32_t crc) {
  return ~crc;
}

SBL_CODE uint8_t SBL_FlashBegin(SBL_FlashSession *session,
                                const SBL_FlashPartition *part,
                                uint32_t size,
                                uint32_t expected_crc) {
  if (!session || !part || size != part->size ||
      part->self_flash || part->runtime_unsafe || !part->allow_flash) {
    return 0U;
  }
  session->part = part;
  session->expected_size = size;
  session->expected_crc = expected_crc;
  session->received = 0U;
  session->running_crc = SBL_FlashCrc32Seed();
  session->current_sector = 0xFFFFFFFFUL;
  session->current_sector_start = 0U;
  session->current_sector_size = 0U;
  session->current_range_start = 0U;
  session->current_range_end = 0U;
  session->suffix_addr = 0U;
  session->suffix_size = 0U;
  session->suffix_buf = 0;
  session->active = 1U;
  return 1U;
}

SBL_CODE uint8_t SBL_FlashErasePartition(const SBL_FlashPartition *part) {
  uint32_t addr;
  uint32_t end;
  if (!part || part->self_flash || part->runtime_unsafe || !part->allow_erase) {
    return 0U;
  }

  addr = part->address;
  end = part->address + part->size;
  if (!SBL_FlashUnlock()) {
    return 0U;
  }
  while (addr < end) {
    const SBL_SectorInfo *sector = sbl_sector_for_addr(addr);
    if (!sector || sector->start != addr) {
      SBL_FlashLock();
      return 0U;
    }
    if (!sbl_flash_erase_sector(sector->sector_index)) {
      SBL_FlashLock();
      return 0U;
    }
    addr += sector->size;
  }
  SBL_FlashLock();
  return 1U;
}

SBL_CODE uint8_t SBL_FlashWriteChunk(SBL_FlashSession *session,
                                     uint32_t offset,
                                     const uint8_t *data,
                                     uint32_t len,
                                     uint32_t chunk_crc) {
  uint32_t chunk_calc;

  if (!session || !session->active || !data || len == 0U) {
    return 0U;
  }
  if ((len & 3U) != 0U || (offset & 3U) != 0U) {
    return 0U;
  }
  if (offset != session->received || (offset + len) > session->expected_size) {
    return 0U;
  }

  chunk_calc = SBL_FlashCrc32Finish(SBL_FlashCrc32Update(SBL_FlashCrc32Seed(), data, len));
  if (chunk_calc != chunk_crc) {
    return 0U;
  }

  while (len > 0U) {
    uint32_t addr = session->part->address + session->received;
    uint32_t piece;
    if (addr < session->current_range_start || addr >= session->current_range_end) {
      if (!sbl_flash_begin_sector(session, addr)) {
        return 0U;
      }
    }
    piece = session->current_range_end - addr;
    if (piece > len) {
      piece = len;
    }

    if (!SBL_FlashUnlock()) {
      return 0U;
    }
    if (!sbl_flash_program_words(addr, data, piece)) {
      SBL_FlashLock();
      return 0U;
    }
    SBL_FlashLock();

    session->running_crc = SBL_FlashCrc32Update(session->running_crc, data, piece);
    session->received += piece;
    data += piece;
    len -= piece;

    if ((session->part->address + session->received) >= session->current_range_end) {
      if (!sbl_flash_finish_sector(session)) {
        return 0U;
      }
      session->current_range_start = 0U;
      session->current_range_end = 0U;
    }
  }

  return 1U;
}

SBL_CODE uint8_t SBL_FlashFinalize(SBL_FlashSession *session) {
  uint32_t verify_crc = SBL_FlashCrc32Seed();
  const uint8_t *ptr;

  if (!session || !session->active || !session->part) {
    return 0U;
  }
  if (session->received != session->expected_size) {
    SBL_FlashAbort(session);
    return 0U;
  }
  if (session->current_range_end != 0U) {
    if (!sbl_flash_finish_sector(session)) {
      SBL_FlashAbort(session);
      return 0U;
    }
  }
  if (SBL_FlashCrc32Finish(session->running_crc) != session->expected_crc) {
    SBL_FlashAbort(session);
    return 0U;
  }

  ptr = (const uint8_t *)session->part->address;
  verify_crc = SBL_FlashCrc32Update(verify_crc, ptr, session->expected_size);
  verify_crc = SBL_FlashCrc32Finish(verify_crc);

  if (verify_crc != session->expected_crc) {
    SBL_FlashAbort(session);
    return 0U;
  }

  session->active = 0U;
  return 1U;
}

SBL_CODE void SBL_FlashAbort(SBL_FlashSession *session) {
  if (!session) {
    return;
  }
  sbl_flash_abort_restore_suffix(session);
  sbl_flash_release_suffix(session);
  session->part = 0;
  session->expected_size = 0U;
  session->expected_crc = 0U;
  session->received = 0U;
  session->running_crc = 0U;
  session->current_sector = 0xFFFFFFFFUL;
  session->current_sector_start = 0U;
  session->current_sector_size = 0U;
  session->current_range_start = 0U;
  session->current_range_end = 0U;
  session->active = 0U;
}
