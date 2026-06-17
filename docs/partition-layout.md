# TOS Flash Partition Layout

STM32F407ZGT6 internal Flash is split on physical erase-sector boundaries.
Every executable image owns its vector table, reset handler, runtime startup,
linker script, and required low-level helpers. Executable partitions exchange
only versioned records through the TEE state area; they do not call functions
by address in another executable partition.

| Partition | Address | Size | Sector(s) | Update policy |
|---|---:|---:|---:|---|
| ELF | `0x08000000` | 16 KiB | 0 | Factory ELF only; immutable root loader |
| SBL | `0x08004000` | 32 KiB | 1-2 | Staged in TMP, applied by ELF |
| TEE | `0x0800C000` | 16 KiB | 3 | Factory manifest + append-only state tail |
| REC | `0x08010000` | 64 KiB | 4 | Staged in TMP, applied by ELF |
| SAH | `0x08020000` | 128 KiB | 5 | Direct data-image update |
| SYSTEM | `0x08040000` | 512 KiB | 6-9 | Direct executable-image update |
| TMP | `0x080C0000` | 128 KiB | 10 | Runtime cache and staged boot image |
| USERDATA | `0x080E0000` | 128 KiB | 11 | Settings and read-only `/data` volume |

## Boot flow

```text
STM32 reset -> ELF -> SBL -> SYSTEM / REC / FASTBOOT
```

ELF accepts only a CRC-checked, bounds-checked SBL or REC transaction whose
source is the TMP staging range. It never accepts ELF or TEE as an update
target. A successful staged update resets and returns to FASTBOOT.

## Filesystem view

SYSTEM always exposes a virtual root:

```text
/data       USERDATA internal FAT volume, read-only in File Manager
/tmp        TMP internal FAT metadata view, read-only in File Manager
/storage    SD card `/storage`, read/write; empty when no SD is mounted
/init       SD marker file, exactly 16 KiB of 0xFF
```

The SD root is restricted to `/storage` and `/init`. Logs and dumps are stored
under `/storage/tos/_` so they remain accessible from an external reader.

TMP staging starts at `0x080C1000`; the first 4 KiB is regenerated FAT metadata.
USERDATA keeps the existing settings journal in the first 32 KiB and exposes a
FAT volume from `0x080E8000` through the end of Flash.

## Build artifacts

Normal export produces only:

```text
sbl.bin  tee.bin  rec.bin  sah.bin  system.bin
```

ELF deliberately has no BIN artifact. `elf_stage.elf` is accepted only by the
explicit factory programming target. Normal runtime and system targets never
perform mass erase and never include sector 0.