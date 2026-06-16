#ifndef SBL_MEM_H
#define SBL_MEM_H

#include "sbl_common.h"

SBL_CODE void SBL_MemReset(void);
SBL_CODE void *SBL_MemAlloc(uint32_t size);
SBL_CODE void SBL_MemFree(void *ptr);

#endif /* SBL_MEM_H */
