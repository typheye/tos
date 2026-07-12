/**
 ******************************************************************************
 * @file    init.h
 * @author  Typheye
 * @brief   SBL boot initialization interface.
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

#ifndef SBL_INIT_H
#define SBL_INIT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void SBL_Main(void);
void SBL_Run(void);
uint8_t SBL_AppLooksValid(void);
#ifdef __cplusplus
}
#endif
#endif /* SBL_INIT_H */
