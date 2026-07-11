#include "mem.h"
#include "dram.h"

SBL_CODE void SBL_MemReset(void) {
  SysDram_Init();
}

SBL_CODE void *SBL_MemAlloc(uint32_t size) {
  return SysDram_AllocFast(size);
}

SBL_CODE void *SBL_MemAllocDma(uint32_t size) {
  return SysDram_AllocDma(size);
}

SBL_CODE void SBL_MemFree(void *ptr) {
  SysDram_Free(ptr);
}
