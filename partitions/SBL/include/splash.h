#ifndef SBL_SPLASH_H
#define SBL_SPLASH_H

#include "common.h"
#include "manifest.h"

#if LCD_ENABLED
SBL_CODE void SBL_SplashRun(void);
#else
#define SBL_SplashRun() do{}while(0)
#endif

#endif /* SBL_SPLASH_H */
