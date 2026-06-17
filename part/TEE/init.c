#include "tee_format.h"

#define TEE_DATA __attribute__((section(".tee.data"), used, aligned(4)))

/* Signature verification is intentionally a format-only placeholder for now.
 * The immutable ELF validates bounds and CRC today. Future factory tooling may
 * fill crc32/signature fields without changing the partition ABI. */
TEE_DATA const TosTeeManifest g_tos_tee_manifest = {
    TOS_TEE_MANIFEST_MAGIC,
    TOS_TEE_MANIFEST_VERSION,
    8U,
    0xFFFFFFFFUL,
    {
        {"elf",      TOS_PART_ELF_OFFSET,      TOS_PART_ELF_SIZE,      0x00000001UL, 0xFFFFFFFFUL},
        {"sbl",      TOS_PART_SBL_OFFSET,      TOS_PART_SBL_SIZE,      0x00000002UL, 0xFFFFFFFFUL},
        {"rec",      TOS_PART_REC_OFFSET,      TOS_PART_REC_SIZE,      0x00000002UL, 0xFFFFFFFFUL},
        {"sah",      TOS_PART_SAH_OFFSET,      TOS_PART_SAH_SIZE,      0x00000004UL, 0xFFFFFFFFUL},
        {"system",   TOS_PART_SYSTEM_OFFSET,   TOS_PART_SYSTEM_SIZE,   0x00000002UL, 0xFFFFFFFFUL},
        {"tmp",      TOS_PART_TMP_OFFSET,      TOS_PART_TMP_SIZE,      0x00000008UL, 0xFFFFFFFFUL},
        {"userdata", TOS_PART_USERDATA_OFFSET, TOS_PART_USERDATA_SIZE, 0x00000010UL, 0xFFFFFFFFUL},
        {"tee",      TOS_PART_TEE_OFFSET,      TOS_PART_TEE_SIZE,      0x00000001UL, 0xFFFFFFFFUL},
    },
    {0},
    {0},
};
