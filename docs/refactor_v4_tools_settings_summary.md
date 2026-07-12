# TOS v4 Settings and Serial Tool Refactor

> Date: 2026-07-12
> Scope: slave-board firmware and win-pc platform tools

## 1. SD settings persistence

- Canonical path is `0:/data/settings.bin`.
- `SM_Init()` installs safe defaults before peripheral startup.
- After SDIO and FatFs mount, `SM_Mount()` creates `0:/data` when needed, then loads settings.
- The SD root cleanup whitelist retains the `data` and `storage` directories.
- The previous root `settings.bin` was deleted by cleanup; this was the main persistence failure.
- The settings structure now stores CRC-32 with the CRC field treated as zero during calculation.
- Exact file length, family magic and CRC are verified before settings are accepted.
- Legacy files with a zero CRC are loaded once and immediately rewritten with a valid CRC.
- SD availability becomes true only after a complete write and successful `f_sync()`.

## 2. SBL serial protocol

- SBL supports `GETVAR product`, `version`, `version-bootloader`, `version-baseband`, `serialno`, `unlocked`, and `partition-size:<name>`.
- `GETVAR all` and `OEM PARTITIONS` enumerate the device-owned partition table.
- Partition records report name, size, flash, erase and staged capabilities.
- The PC tool no longer contains a compiled-in partition table.
- The SBL serial-number response now waits between CDC packets, preventing the UID from being dropped.

## 3. Windows tools

- `tsblboot` was renamed to `sbltool`, including package, executable metadata, build and install scripts.
- `tdb devices` probes every COM port with `HELLO` and accepts only `TDB/1 REC <UID>`.
- `sbltool devices` probes every COM port with `GETVAR serialno` and `GETVAR product`.
- Both tools print the 24-hex STM32 UID in adb/fastboot-style device rows.
- `-s <serial>` selects a device; `--port` remains hidden only for diagnostics and compatibility.
- Public SBL commands use `getvar`, `flash`, `erase`, `reboot`, `reboot recovery`, `reboot bootloader`, and `oem`.

## 4. LCD-independent partition build

- `LCD_ENABLED` is now an overridable board/toolchain setting with default value 1.
- A clean Headless build with `LCD_ENABLED=0` linked ELF, SBL, REC and SYSTEM and verified all signatures.
- Headless SBL usage was 26240 bytes of 49152 bytes; normal SBL usage was 48068 bytes.

## 5. Build-system and cleanup fixes

- Secure-boot key preparation now creates the parent directory of `signing-key.path` in clean build trees.
- Obsolete SBL TEE/SAH partition-name constants and obsolete PC partition entries were removed.
- CubeMX generated source files were not edited.

## 6. Verification checklist

- Release factory images build and SBL/REC/SYSTEM signature verification pass.
- Headless factory images build and signature verification pass.
- TDB negative discovery ignores SYSTEM and unrelated COM ports.
- TDB positive discovery returns the board UID in REC.
- SBL positive discovery returns the same UID in FASTBOOT.
- Final integration verifies `0:/data/settings.bin` by TDB pull and host-side CRC calculation.


## 7. Final hardware evidence

- Board UID: `002E00544152500A20303754`.
- `sbltool devices` reported the board as `fastboot transport_id:COM8`.
- `getvar product` returned `CNAEK7`; `getvar partition-size:system` returned `0x000C0000`.
- `getvar all` and `partitions` returned SBL, REC, SYSTEM and TMP directly from the device table.
- `tdb devices` reported the same UID as `device ... transport_id:COM12`.
- TDB `stat` and `pull` confirmed `0:/data/settings.bin` is a 712-byte file.
- First readback CRC: stored and calculated `0xA901B8AB`.
- Second-boot CRC: stored and calculated `0x93EEE827`.
- Across reboot only `wlan_on` changed from 0 to 1 plus the four CRC bytes; both files remained valid.
- Final SYSTEM PC was `0x080571CE`; the ELF state area was restored to erased `0xFFFFFFFF` words.
## 8. File manager `/data` mapping fix

- `settings.bin` was never marked hidden; FatFs reported attribute `0x20` (Archive only).
- The missing UI entry was caused by the old `/data` virtual-directory stub in `FMCore_ListDir()`.
- `/data` previously translated to an empty path, always returned zero entries, and was write-protected.
- `/data` now maps to the SD path `0:/data`; directory listing, stat, read, create, write and delete use the real SD directory.
- The rebuilt SYSTEM was flashed successfully and reached PC `0x08052682`.
## 9. CLI and recovery follow-up

- Fixed ELF consumption of targeted RESTART records; recovery/fastboot targets remain valid until SBL/REC consumes them.
- Verified `sbltool reboot recovery` enters REC and `tdb devices` discovers the board.
- `sbltool devices` now prints only `<serial>\tfastboot`.
- `tdb devices` now prints the adb header and only `<serial>\tdevice` per device.
- `sbltool getvar` without a variable prints usage; `getvar all` prints product/version/serial/unlocked rather than the partition table.
- TDB `ls` prints directories with `/` and files as `<name>\t<size> bytes` on separate lines.
- TDB interactive shell uses remote `ls` results for Tab completion; Windows packaging includes `pyreadline3`.
- The main file manager root now maps directly to the SD root and no longer exposes a virtual directory layer.
- Final packaged `sbltool.exe` and `tdb.exe` smoke tests passed; final SYSTEM PC was `0x08056A3E`.