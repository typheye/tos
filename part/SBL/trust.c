#include <stdint.h>
#include "sbl_common.h"
#include "tos_partitions.h"

#define SBL_TRUST_MAGIC   0x54525553UL /* TRUS */
#define SBL_TRUST_VERSION 2UL

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
    SBL_TRUST_MAGIC, SBL_TRUST_VERSION, 4U, 0xFFFFFFFFUL,
    {
        {"sbl", TOS_PART_SBL_OFFSET, TOS_PART_SBL_SIZE, 0xFFFFFFFFUL},
        {"rec", TOS_PART_REC_OFFSET, TOS_PART_REC_SIZE, 0xFFFFFFFFUL},
        {"sah", TOS_PART_SAH_OFFSET, TOS_PART_SAH_SIZE, 0xFFFFFFFFUL},
        {"system", TOS_PART_SYSTEM_OFFSET, TOS_PART_SYSTEM_SIZE, 0xFFFFFFFFUL},
    },
};
