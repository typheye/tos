#include "sbl_mem.h"

#define SBL_HEAP_BASE 0x10000000UL
#define SBL_HEAP_SIZE 4096UL

static uint32_t sbl_heap_used;

SBL_CODE void SBL_MemReset(void) {
  sbl_heap_used = 0U;
}

SBL_CODE void *SBL_MemAlloc(uint32_t size) {
  uint32_t aligned = (size + 7U) & ~7U;
  if (aligned == 0U || sbl_heap_used + aligned > SBL_HEAP_SIZE) {
    return 0;
  }
  void *p = (void *)(SBL_HEAP_BASE + sbl_heap_used);
  sbl_heap_used += aligned;
  return p;
}
