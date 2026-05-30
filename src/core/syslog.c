/**
 * @file    syslog.c
 * @brief   Placeholder — logging currently done via printf-direct macros in syslog.h
 *
 * SysLog_Write() kept for future use when a more sophisticated formatter is needed.
 */

#include "include/syslog.h"

__attribute__((weak))
uint32_t SysLog_GetTick(void) { return 0; }

void SysLog_Write(SysLog_Level_t level, const char *mod, const char *task,
                  const char *file, int line, const char *fmt, ...) {
  (void)level; (void)mod; (void)task; (void)file; (void)line; (void)fmt;
  /* Not used while macros bypass to printf directly.
   * Restore macro definitions in syslog.h to re-enable this path. */
}
