/**
 ******************************************************************************
 * @file    emotion_manager.h
 * @author  Typheye
 * @brief   Emotion Manager interface.
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

#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/sys/include/syslog.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

void EmotionManager_Init(void);

bool EmotionManager_SetExpression(const char *expr);
void EmotionManager_SetAuto(void);

bool EmotionManager_IsManual(void);
const char *EmotionManager_GetExpression(void);
const char *EmotionManager_GetReportExpression(void);
uint32_t EmotionManager_GetRevision(void);

void EmotionManager_ReportAutoExpression(const char *expr);
void EmotionManager_Tick(void);

#ifdef __cplusplus
}
#endif

#endif /* EMOTION_MANAGER_H */
