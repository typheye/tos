#include "sbl_mem.h"
#include "core/sys/include/sysdram.h"

SBL_CODE void SBL_MemReset(void) {
  SysDram_Init();
}

SBL_CODE void *SBL_MemAlloc(uint32_t size) {
  return SysDram_AllocFast(size);
}
