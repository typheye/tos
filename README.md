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
