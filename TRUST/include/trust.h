#ifndef TRUST_H
#define TRUST_H

#include <stdint.h>

#define TRUST_MAGIC   0x54525553UL
#define TRUST_VERSION 0x00010000UL

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t flags;
  uint32_t reserved0;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t crc;
} Trust_Block_t;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t Trust_CalcCrc(const Trust_Block_t *block);

#ifdef __cplusplus
}
#endif

#endif /* TRUST_H */
