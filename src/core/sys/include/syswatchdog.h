#ifndef __SYSWATCHDOG_H
#define __SYSWATCHDOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IWDG-only watchdog integration layer.
 * This layer refreshes IWDG, records IWDG reset reason, and provides a small
 * system-service tick used by long network loops.
 */
void SysWatchdog_Init(void);
void SysWatchdog_FeedNow(void);
void SysWatchdog_Tick(void);
void SysWatchdog_ShowBootReasonIfAny(void);
uint32_t SysWatchdog_GetBootCode(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSWATCHDOG_H */
