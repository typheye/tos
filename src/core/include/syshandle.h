#ifndef __SYSHANDLE_H
#define __SYSHANDLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* System exception/error codes.
 * Keep values stable: these codes are shown on the fatal UI and can be used
 * later for log upload or customer support.
 */
#define SYS_ERR_NONE                  0x00000000UL
#define SYS_ERR_SD_NOT_READY          0x00001001UL
#define SYS_ERR_SD_TIMEOUT            0x00001002UL
#define SYS_ERR_SD_DISK_ERR           0x00001003UL
#define SYS_ERR_SD_LOST               0x00001004UL
#define SYS_ERR_SD_NO_FILESYSTEM      0x00001005UL
#define SYS_ERR_SD_FORMAT_FAILED      0x00001006UL
#define SYS_ERR_SD_INIT_FAILED        0x00001007UL
#define SYS_ERR_SD_BROWSER_FAILED     0x00001008UL

/* Show the fatal exception UI, count down 5 seconds, then reset.
 * This function does not return under normal conditions.
 */
void SysHandle_Exception(uint32_t code);
void SysHandle_Fatal(uint32_t code);
uint32_t SysHandle_GetLastCode(void);
const char *SysHandle_CodeName(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif /* __SYSHANDLE_H */
