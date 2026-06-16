#include "trust.h"

#define TRUST_CODE_ATTR  __attribute__((section(".sbl.text"), noinline, used))
#define TRUST_DATA_ATTR  __attribute__((section(".trust.rodata"), used))

TRUST_CODE_ATTR uint32_t Trust_CalcCrc(const Trust_Block_t *block) {
  return block->magic ^ block->version ^ block->flags ^
         block->reserved0 ^ block->reserved1 ^ block->reserved2 ^
         0xA55AA55AUL;
}

static const Trust_Block_t g_trust_block TRUST_DATA_ATTR = {
    TRUST_MAGIC,
    TRUST_VERSION,
    0x00000001UL,
    0x13572468UL,
    0x24681357UL,
    0x55AA55AAUL,
    TRUST_MAGIC ^ TRUST_VERSION ^ 0x00000001UL ^
        0x13572468UL ^ 0x24681357UL ^ 0x55AA55AAUL ^ 0xA55AA55AUL,
};
