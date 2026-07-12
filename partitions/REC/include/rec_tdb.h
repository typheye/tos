/**
 ******************************************************************************
 * @file    rec_tdb.h
 * @author  Typheye
 * @brief   REC TDB USB CDC interface.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */

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
