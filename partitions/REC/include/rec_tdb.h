#ifndef REC_TDB_H
#define REC_TDB_H

#include <stdint.h>
#include "rec.h"

REC_CODE uint8_t REC_TDB_Start(void);
REC_CODE void REC_TDB_Stop(void);
REC_CODE void REC_TDB_Tick(void);
REC_CODE uint8_t REC_TDB_IsStarted(void);
REC_CODE uint8_t REC_TDB_IsConfigured(void);

#endif /* REC_TDB_H */
