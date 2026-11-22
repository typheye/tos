# -*- mode: python ; coding: utf-8 -*-
"""Native one-file builds. Metadata is the only version source."""
from pathlib import Path
import json
import sys

root = Path(SPECPATH).parent
src = root / "src"
windows = sys.platform == "win32"
if sys.platform not in ("win32", "linux"):
    raise RuntimeError(f"Unsupported build platform: {sys.platform}")

for tool in ("tos_helper", "tvidtool", "sbltool", "tdb"):
    package = src / tool
    meta = json.loads((package / "metadata.json").read_text(encoding="utf-8"))
    datas = [(str(package / "metadata.json"), tool)]
    hidden = {"tos_helper": ["hid", "tkinter", "tkinter.ttk"],
              "tvidtool": ["cv2", "numpy"],
              "sbltool": ["serial.tools.list_ports"],
              "tdb": ["serial.tools.list_ports", "readline"]}[tool]
    if windows and tool == "tdb":
        hidden.append("pyreadline3")
    if tool == "tos_helper":
        datas.append((str(package / "assets"), "tos_helper/assets"))
    a = Analysis([str(package / "app.py")], pathex=[str(src)], binaries=[],
                 datas=datas, hiddenimports=hidden, hookspath=[], hooksconfig={},
                 runtime_hooks=[], excludes=[], noarchive=False, optimize=0)
    pyz = PYZ(a.pure)
    native = {}
    if windows:
        from PyInstaller.utils.win32.versioninfo import (
            VSVersionInfo, FixedFileInfo, StringFileInfo, StringTable,
            StringStruct, VarFileInfo, VarStruct,
        )
        version = tuple(int(x) for x in meta["version"].split("."))
        version = (version + (0, 0, 0, 0))[:4]
        strings = {"CompanyName": meta["developer"],
                   "FileDescription": meta["description"],
                   "FileVersion": meta["version"], "ProductVersion": meta["version"],
                   "InternalName": tool, "OriginalFilename": meta["executable"] + ".exe",
                   "ProductName": meta["display_name"],
                   "LegalCopyright": "Copyright (c) 2021-2026 Typheye"}
        native["version"] = VSVersionInfo(
            ffi=FixedFileInfo(filevers=version, prodvers=version, mask=0x3f, flags=0,
                              OS=0x40004, fileType=1, subtype=0, date=(0, 0)),
            kids=[StringFileInfo([StringTable("040904B0", [StringStruct(k, v)
                   for k, v in strings.items()])]),
                  VarFileInfo([VarStruct("Translation", [1033, 1200])])])
        native["icon"] = str(package / "assets" / "typheye_rounded.ico") if tool == "tos_helper" else "NONE"
        if tool == "tos_helper":
            native["manifest"] = str(package / "tos_helper.manifest")
    # Keep CLI diagnostics available on both hosts. Windows shortcuts hide the console.
    exe = EXE(pyz, a.scripts, a.binaries, a.datas, [],
              name=meta["executable"], console=True, debug=False,
              bootloader_ignore_signals=False, strip=False, upx=False,
              disable_windowed_traceback=False, **native)
