/**
 ******************************************************************************
 * @file    build.h
 * @author  Typheye
 * @brief   SBL build-time constants interface.
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

#ifndef SBL_BUILD_H
#define SBL_BUILD_H

#include "manifest.h"

#define SBL_BUILD_PRODUCT_NAME          TOS_PRODUCT_NAME
#define SBL_BUILD_VERSION               TOS_VERSION
#define SBL_BUILD_VERSION_BOOTLOADER    TOS_BL_VERSION
#define SBL_BUILD_VERSION_BASEBAND      TOS_BB_VERSION

#endif /* SBL_BUILD_H */
