/**
 ******************************************************************************
 * @file    ui.h
 * @author  Typheye
 * @brief   SBL fastboot UI and recovery exception interface.
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
#ifndef SBL_UI_H
#define SBL_UI_H

#include "common.h"
#include "secure_boot.h"

SBL_CODE void SBL_UiDrawFastboot(void);
SBL_CODE void SBL_UiRunFastboot(void);
SBL_CODE void SBL_UiRunSystemDamage(SecureBoot_Result_t reason);
SBL_CODE void SBL_UiRunRecoveryException(void);

#endif /* SBL_UI_H */
