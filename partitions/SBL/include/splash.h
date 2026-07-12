/**
 ******************************************************************************
 * @file    splash.h
 * @author  Typheye
 * @brief   SBL splash screen interface.
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
