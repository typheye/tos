#ifndef SBL_FLASH_H
#define SBL_FLASH_H

#include "sbl_common.h"

#define SBL_FLASH_CHUNK_SIZE (16UL * 1024UL)

typedef struct {
  const char *name;
  uint32_t offset;
  uint32_t address;       /* final partition address */
  uint32_t write_address; /* direct destination or TMP staging address */
  uint32_t size;
  uint8_t staged;
  uint8_t allow_flash;
  uint8_t allow_erase;
  uint8_t reserved;
} SBL_FlashPartition;

typedef struct {
  const SBL_FlashPartition *part;
  uint32_t expected_size;
  uint32_t expected_crc;
  uint32_t received;
  uint32_t running_crc;
  uint32_t write_address;
  uint32_t post_boot_target;
  uint8_t active;
  uint8_t requires_reset;
} SBL_FlashSession;

SBL_CODE const SBL_FlashPartition *SBL_FlashFindPartition(const char *name);
SBL_CODE uint32_t SBL_FlashChunkSize(void);
SBL_CODE uint32_t SBL_FlashCrc32Seed(void);
SBL_CODE uint32_t SBL_FlashCrc32Update(uint32_t crc, const uint8_t *data, uint32_t len);
SBL_CODE uint32_t SBL_FlashCrc32Finish(uint32_t crc);
SBL_CODE uint8_t SBL_FlashBegin(SBL_FlashSession *session,
                                const SBL_FlashPartition *part,
                                uint32_t size,
                                uint32_t expected_crc);
SBL_CODE uint8_t SBL_FlashErasePartition(const SBL_FlashPartition *part);
SBL_CODE uint8_t SBL_FlashWriteChunk(SBL_FlashSession *session,
                                     uint32_t offset,
                                     const uint8_t *data,
                                     uint32_t len,
                                     uint32_t chunk_crc);
SBL_CODE void SBL_FlashSetPostBootTarget(SBL_FlashSession *session,
                                         uint32_t target);
SBL_CODE uint8_t SBL_FlashFinalize(SBL_FlashSession *session);
SBL_CODE void SBL_FlashAbort(SBL_FlashSession *session);

#endif /* SBL_FLASH_H */
