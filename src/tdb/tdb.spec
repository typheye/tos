# -*- mode: python ; coding: utf-8 -*-
from pathlib import Path

package_dir = Path(SPECPATH)
source_root = package_dir.parent

a = Analysis(
    [str(package_dir / "app.py")],
    pathex=[str(source_root)],
    binaries=[],
    datas=[],
    hiddenimports=["serial.tools.list_ports"],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)
exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="tdb",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon="NONE",
    version=str(package_dir / "version_info.txt"),
)
