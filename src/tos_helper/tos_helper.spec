# -*- mode: python ; coding: utf-8 -*-
from pathlib import Path

package_dir = Path(SPECPATH)
source_root = package_dir.parent
assets_dir = package_dir / "assets"

a = Analysis(
    [str(package_dir / "app.py")],
    pathex=[str(source_root)],
    binaries=[],
    datas=[(str(assets_dir / "typheye_rounded.png"), "tos_helper/assets")],
    hiddenimports=["hid", "tkinter", "tkinter.ttk"],
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
    name="TOS Helper",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=str(assets_dir / "typheye_rounded.ico"),
    version=str(package_dir / "version_info.txt"),
    manifest=str(package_dir / "tos_helper.manifest"),
)
