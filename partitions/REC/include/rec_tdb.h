/**
 ******************************************************************************
 * @file    rec_tdb.h
 * @author  Typheye
 * @brief   REC TDB USB CDC interface.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
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
