#include <stdint.h>

#include "sbl_common.h"

#define SBL_TRUST_MAGIC   0x54525553UL /* TRUS */
#define SBL_TRUST_VERSION 1UL

typedef struct {
  char name[12];
  uint32_t offset;
  uint32_t size;
  uint32_t crc32;
} SBL_TrustPartition;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t partition_count;
  uint32_t reserved;
  SBL_TrustPartition part[4];
} SBL_TrustManifest;

SBL_CONST const SBL_TrustManifest sbl_trust_manifest = {
    SBL_TRUST_MAGIC,
    SBL_TRUST_VERSION,
    4U,
    0xFFFFFFFFUL,
    {
        {"sbl",    0x00000000UL, 0x00010000UL, 0xFFFFFFFFUL},
        {"sre",    0x00010000UL, 0x00010000UL, 0xFFFFFFFFUL},
        {"sah",    0x00020000UL, 0x00020000UL, 0xFFFFFFFFUL},
        {"system", 0x00040000UL, 0x00080000UL, 0xFFFFFFFFUL},
    },
};
