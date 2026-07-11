#ifndef TOS_SHA256_H
#define TOS_SHA256_H

#include <stdint.h>

typedef struct {
  uint32_t state[8];
  uint64_t total_size;
  uint8_t block[64];
  uint32_t block_size;
} TosSha256Context;

void TosSha256Init(TosSha256Context *ctx);
void TosSha256Update(TosSha256Context *ctx, const void *data, uint32_t size);
void TosSha256Final(TosSha256Context *ctx, uint8_t digest[32]);

#endif /* TOS_SHA256_H */
