#ifndef REC_H
#define REC_H

#include <stdint.h>

#define SRE_CODE  __attribute__((section(".sre.text"), noinline, used))
#define SRE_CONST __attribute__((section(".sre.rodata"), used))

SRE_CODE void SRE_Run(uint8_t mode);
SRE_CODE uint8_t SRE_FatProbeInit(void);
SRE_CODE uint8_t SRE_FatHasUpgradeManifest(void);
SRE_CODE uint8_t SRE_FatFlashUpgrade(void (*status)(const char *, uint16_t));

#define REC_MODE_WAIT    0U
#define REC_MODE_FORMAT  1U
#define REC_MODE_UPGRADE 2U

#endif /* REC_H */
