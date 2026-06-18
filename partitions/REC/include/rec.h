#ifndef REC_H
#define REC_H

#include <stdint.h>

#define REC_CODE  __attribute__((section(".rec.text"), noinline, used))
#define REC_CONST __attribute__((section(".rec.rodata"), used))

REC_CODE void REC_Run(uint8_t mode);
void REC_Main(uint8_t clock_ok);
REC_CODE uint8_t REC_FatProbeInit(void);
REC_CODE uint8_t REC_FatHasUpgradeManifest(void);
REC_CODE uint8_t REC_FatFlashUpgrade(void (*status)(const char *, uint16_t));
REC_CODE uint8_t REC_FatFormat(void);
REC_CODE uint8_t REC_FatInitStorage(void);
REC_CODE uint8_t REC_FatPrepareMsc(uint32_t *block_count);
REC_CODE void REC_FatRelease(void);
REC_CODE const char *REC_FatLastError(void);

/* Raw 512-byte block access used by REC USB MSC.  These wrappers deliberately
 * reuse the same SDIO initialization/retry path as REC FatFs so the two REC
 * storage users cannot drift into subtly different card geometry or bus
 * setup.  All calls are blocking and therefore must run from REC main context,
 * never from a USB interrupt callback. */
REC_CODE uint8_t REC_BlockInit(uint32_t *block_count);
REC_CODE uint8_t REC_BlockReady(void);
REC_CODE uint8_t REC_BlockRead(uint32_t lba, uint8_t *buffer,
                               uint32_t block_count);
REC_CODE uint8_t REC_BlockWrite(uint32_t lba, const uint8_t *buffer,
                                uint32_t block_count);
REC_CODE uint8_t REC_BlockSync(void);
REC_CODE void REC_BlockRelease(void);

#define REC_MODE_WAIT    0U
#define REC_MODE_FORMAT  1U
#define REC_MODE_UPGRADE 2U
#define REC_MODE_CLOCK_ERROR 3U
#define REC_MODE_INIT 4U

#endif /* REC_H */
