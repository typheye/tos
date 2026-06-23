# TOS Windows Tools

## Structure

- `src/tos_helper/` — TOS Helper GUI, formerly `hidtools.py`
- `src/tsblboot/` — SBL boot/flash CLI, formerly `sbltools.py`
- `src/tdb/` — TOS Debug Bridge CLI
- `scripts/` — Conda setup, build, clean, CLI install and flash scripts

## Build

From Anaconda Prompt:

```bat
cd /d C:\Code\tos\win-pc
scripts\setup-conda.bat
```

Or with an existing `tos` environment:

```bat
conda activate tos
python -m pip install -r requirement.txt
scripts\build.bat --install
```

Outputs:

```text
dist/TOS Helper.exe
dist/platform-tools/tsblboot.exe
dist/platform-tools/tdb.exe
```

After `--install`, the two CLI tools are available directly while the `tos` environment is active:

```bat
tsblboot partitions
tdb --version
```

## Package metadata

Each executable keeps its PyInstaller spec and Windows version resource beside its source package:

```text
src/tos_helper/tos_helper.spec
src/tos_helper/version_info.txt
src/tsblboot/tsblboot.spec
src/tsblboot/version_info.txt
src/tdb/tdb.spec
src/tdb/version_info.txt
```

`TOS Helper.exe` uses a Per-Monitor-V2 DPI manifest and the Typheye application icon from `src/tos_helper/assets/`.
