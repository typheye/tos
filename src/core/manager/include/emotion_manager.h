/**
 ******************************************************************************
 * @file    emotion_manager.h
 * @author  Typheye
 * @brief   Emotion Manager interface.
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

#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

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
