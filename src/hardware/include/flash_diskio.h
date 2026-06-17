#ifndef TOS_FLASH_DISKIO_H
#define TOS_FLASH_DISKIO_H

#include <stdint.h>
#include "ff.h"
#include "ff_gen_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const Diskio_drvTypeDef TOS_FlashDisk_Driver;
extern char TMPPath[4];
extern char DataPath[4];
extern FATFS TMPFatFS;
extern FATFS DataFatFS;

/* Link the two internal read-only volumes after the SD driver. */
uint8_t FlashDiskIO_LinkVolumes(void);

/* Create valid FAT12 metadata when a volume is blank. Existing valid volumes
 * are never erased. USERDATA settings are preserved during first migration. */
uint8_t FlashDiskIO_EnsureVolumes(void);

/* Called immediately after the owning physical sector has been erased. */
uint8_t FlashDiskIO_RebuildTmpAfterErase(void);
uint8_t FlashDiskIO_RebuildUserdataAfterErase(void);

#ifdef __cplusplus
}
#endif

#endif /* TOS_FLASH_DISKIO_H */
