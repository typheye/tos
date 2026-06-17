#ifndef REC_MSC_H
#define REC_MSC_H

#include <stdint.h>

#include "rec.h"

REC_CODE uint8_t REC_MSC_Start(void);
REC_CODE void REC_MSC_Stop(void);
REC_CODE void REC_MSC_Tick(void);
REC_CODE uint8_t REC_MSC_IsStarted(void);
REC_CODE uint8_t REC_MSC_IsConfigured(void);

#endif /* REC_MSC_H */
