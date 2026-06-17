#ifndef SRE_H
#define SRE_H

#include <stdint.h>

#define SRE_CODE  __attribute__((section(".sre.text"), noinline, used))
#define SRE_CONST __attribute__((section(".sre.rodata"), used))

SRE_CODE void SRE_Run(uint8_t auto_format);

#endif /* SRE_H */
