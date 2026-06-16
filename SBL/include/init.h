#ifndef SBL_INIT_H
#define SBL_INIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SBL_Run(void);
uint8_t SBL_AppLooksValid(void);
uint8_t SBL_TrustLooksValid(void);

#ifdef __cplusplus
}
#endif

#endif /* SBL_INIT_H */
