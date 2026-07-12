#ifndef TOS_MANIFEST_H
#define TOS_MANIFEST_H

#include <stddef.h>
#include <stdint.h>
#include "tee_format.h"

/* ======================================================================
 *  TOS Master Manifest — single source of truth for all partition config
 *  Partition order matches boot sequence: ELF → SBL → REC → SYSTEM
 * ====================================================================== */

/* ── Board identity ───────────────────────────────────────────────── */
#define TOS_PRODUCT_NAME    "CNAEK7"
#define TOS_HW_REV          "V1"
#define TOS_MODEL           0x544F5301UL  /* "TOS\1" */

/* ── Device IDs (ADB / Fastboot compatible) ───────────────────────── */
/* 16-byte unique device serial, stored at OTG base if available,
 *  fallback: STM32 UID (96-bit) expanded to 128-bit with revision.   */
#define TOS_SERIAL_LEN      16
/* Offsets in bootloader state area for serial persistence */
#define TOS_SERIAL_ADDR     (TOS_PART_ELF_ADDRESS + TOS_PART_ELF_CODE_SIZE + 0x100)
/* ADB vendor ID (0x18D1 = Google, 0x2A45 or custom) */
#define TOS_USB_VID         0xFAED
#define TOS_USB_PID         0x4870

/* ── Firmware versions ────────────────────────────────────────────── */
#define TOS_VERSION          "1"
#define TOS_VERSION_CODE     260615000UL
#define TOS_BUILD            "1.0.0"
#define TOS_PATCH            "2026-01-01"

/* ── Bootloader versions (reported by fastboot getvar) ────────────── */
#define TOS_BL_VERSION       "TOSV1.0"
#define TOS_BB_VERSION       "N/A"       /* baseband */
#define TOS_SECURE_STATE     "yes"       /* "yes" = locked */

/* ── ELF  partition ───────────────────────────────────────────────── */
/* Sector 0, 16 KB immutable.
 * Code 12 KB + state ring 4 KB at tail.
 * ELF never writes its own sector; state records use 1→0 writes.      */
#define TOS_ELF_CODE_SIZE        0x00003000UL

/* ── SBL  partition ───────────────────────────────────────────────── */
/* Sectors 1-3, 48 KB.
 * Code ~30 KB + logo ~18 KB.  Read-only at runtime.
 * Upgrade is staged via TMP → ELF copies.
 * LCD splash logo: disable LCD_ENABLED for headless boards.            */
#ifndef LCD_ENABLED
#define LCD_ENABLED                1
#endif
#define BTN_PORT                   GPIOA
#define BTN_PIN                    15U

/* ── REC  partition ───────────────────────────────────────────────── */
/* Sector 4, 64 KB. Recovery image, read-only at runtime.
 * Upgrade is staged via TMP → ELF copies.                              */

/* ── TMP  partition ────────────────────────────────────────────────── */
/* Sector 5, 128 KB. Upgrade staging area, volatile.
 * Holds one new SBL or REC image before ELF copies it into place.
 * Also used by FASTBOOT 'download' buffer.                             */

/* ── SYSTEM  partition ─────────────────────────────────────────────── */
/* Sectors 6-11, 768 KB. Filesystem-managed persistent storage.
 * No raw-flash settings; all config lives on SD card via FatFS.        */

/* ======================================================================
 *  Partition layout (DO NOT EDIT — kept in sync with ld/ linker scripts)
 * ====================================================================== */
#define TOS_FLASH_BASE              0x08000000UL
#define TOS_FLASH_SIZE              0x00100000UL

/* ELF */
#define TOS_PART_ELF_OFFSET         0x00000000UL
#define TOS_PART_ELF_ADDRESS        (TOS_FLASH_BASE + TOS_PART_ELF_OFFSET)
#define TOS_PART_ELF_SIZE           0x00004000UL
#define TOS_TEE_STATE_ADDRESS       (TOS_PART_ELF_ADDRESS + TOS_ELF_CODE_SIZE)
#define TOS_TEE_STATE_SIZE          0x00001000UL

/* SBL */
#define TOS_PART_SBL_OFFSET         0x00004000UL
#define TOS_PART_SBL_ADDRESS        (TOS_FLASH_BASE + TOS_PART_SBL_OFFSET)
#define TOS_PART_SBL_SIZE           0x0000C000UL

/* REC */
#define TOS_PART_REC_OFFSET         0x00010000UL
#define TOS_PART_REC_ADDRESS        (TOS_FLASH_BASE + TOS_PART_REC_OFFSET)
#define TOS_PART_REC_SIZE           0x00010000UL
#define TOS_REC_COMMAND_SIZE        0x00000400UL
#define TOS_REC_COMMAND_ADDRESS     (TOS_PART_REC_ADDRESS + TOS_PART_REC_SIZE - TOS_REC_COMMAND_SIZE)
#define TOS_PART_REC_SIGNED_SIZE    (TOS_PART_REC_SIZE - TOS_REC_COMMAND_SIZE)

/* TMP */
#define TOS_PART_TMP_OFFSET         0x00020000UL
#define TOS_PART_TMP_ADDRESS        (TOS_FLASH_BASE + TOS_PART_TMP_OFFSET)
#define TOS_PART_TMP_SIZE           0x00020000UL
#define TOS_TMP_STAGE_ADDRESS       TOS_PART_TMP_ADDRESS
#define TOS_TMP_STAGE_SIZE          TOS_PART_TMP_SIZE

/* SYSTEM */
#define TOS_PART_SYSTEM_OFFSET      0x00040000UL
#define TOS_PART_SYSTEM_ADDRESS     (TOS_FLASH_BASE + TOS_PART_SYSTEM_OFFSET)
#define TOS_PART_SYSTEM_SIZE        0x000C0000UL
#define TOS_PART_SYSTEM_SIGNED_SIZE 0x000C0000UL

/* ======================================================================
 *  Boot targets / update kinds
 * ====================================================================== */
#define TOS_BOOT_TARGET_NONE             0xFFFFFFFFUL
#define TOS_BOOT_TARGET_FASTBOOT         0x46424F54UL /* FBOT */
#define TOS_BOOT_TARGET_RECOVERY         0x52454356UL /* RECV */
#define TOS_BOOT_TARGET_RECOVERY_FORMAT  0x52464D54UL /* RFMT */
#define TOS_BOOT_TARGET_RECOVERY_UPGRADE 0x52555047UL /* RUPG */
#define TOS_BOOT_TARGET_RECOVERY_INIT    0x52494E49UL /* RINI */

#define TOS_UPDATE_NONE                 0UL
#define TOS_UPDATE_SBL                  1UL
#define TOS_UPDATE_REC                  2UL

#define TOS_SD_INIT_FILE_SIZE           TOS_PART_ELF_SIZE
#define TOS_USERDATA_SETTINGS_SIZE      0x00008000UL
#define TOS_PART_USERDATA_ADDRESS       (TOS_PART_SYSTEM_ADDRESS + TOS_PART_SYSTEM_SIZE - TOS_USERDATA_SETTINGS_SIZE)

#endif /* TOS_MANIFEST_H */
