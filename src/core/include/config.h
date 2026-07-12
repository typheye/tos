/**
 ******************************************************************************
 * @file    config.h
 * @author  Typheye
 * @brief   Config interface.
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

#ifndef CONFIG_H
#define CONFIG_H

/* ── Hardware ── */
#define CFG_MODEL "TOS-CNAEK7"
#define CFG_MCU "STM32F407"
#define CFG_RAM "192K"
#define CFG_ROM "1024K"

/* ── Firmware ── */
#define CFG_TOS_VERSION "1"
#define CFG_VERSION_CODE 260615000
#define CFG_BUILD "1.26.6.r2"
#define CFG_PATCH "2026-06-01"

/* ── Hardware revision ── */
#define CFG_HW_REV "V1"

#endif
