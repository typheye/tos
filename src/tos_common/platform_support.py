"""Host-specific desktop and USB diagnostics; no device access on import."""
from __future__ import annotations

import getpass
import logging
import sys

LOG = logging.getLogger(__name__)
APP_ID = "Typheye.TOSHelper"
WINDOW_CLASS = "Toshelper"


def permission_denied(exc: BaseException | None) -> bool:
    seen = set()
    while exc is not None and id(exc) not in seen:
        seen.add(id(exc))
        if isinstance(exc, PermissionError) or getattr(exc, "errno", None) in (1, 13):
            return True
        exc = exc.__cause__ or exc.__context__
    return False


def usb_permission_hint(port: str) -> str:
    prefix = f"cannot open {port}: permission denied.\n"
    if sys.platform == "win32":
        return (prefix + "  Close other serial monitors using this COM port, reconnect the board,\n"
                "  and check the USB CDC driver in Device Manager.")
    if sys.platform != "linux":
        return prefix + "  Check device permissions and close other serial monitors."
    import grp
    import os
    import shlex
    user = getpass.getuser()
    try:
        member = user in grp.getgrnam("dialout").gr_mem
    except KeyError:
        member = False
    if member:
        if grp.getgrnam("dialout").gr_gid in os.getgroups():
            return prefix + "  Check this device's udev permissions and close other serial monitors."
        return (prefix + f"  user '{user}' is in the 'dialout' group, but this login session\n"
                "  started before that change. Log out of the desktop and back in\n"
                "  (or reboot), then replug the board. A new terminal is not enough.")
    return (prefix + f"  user '{user}' has no access to the USB CDC device.\n"
            f"  Run once: sudo usermod -aG dialout {shlex.quote(user)}\n"
            "  then fully log out of the desktop and back in (or reboot), and\n"
            "  replug the board. A new terminal is not enough: groups are set at login.")


def enable_windows_high_dpi() -> None:
    """Enable native DPI before Tk and set the Windows taskbar application ID."""
    if sys.platform != "win32":
        return
    import ctypes
    from ctypes import wintypes
    try:
        shell32 = ctypes.WinDLL("shell32", use_last_error=True)
        set_id = shell32.SetCurrentProcessExplicitAppUserModelID
        set_id.argtypes = [wintypes.LPCWSTR]
        set_id.restype = ctypes.c_long
        set_id(APP_ID)
    except (AttributeError, OSError) as exc:
        LOG.warning("Windows taskbar identity unavailable: %s", exc)
    try:
        user32 = ctypes.WinDLL("user32", use_last_error=True)
        set_dpi = user32.SetProcessDpiAwarenessContext
        set_dpi.argtypes = [ctypes.c_void_p]
        set_dpi.restype = wintypes.BOOL
        if set_dpi(ctypes.c_void_p(-4)):
            return
    except (AttributeError, OSError):
        pass
    try:
        shcore = ctypes.WinDLL("shcore", use_last_error=True)
        shcore.SetProcessDpiAwareness.argtypes = [ctypes.c_int]
        shcore.SetProcessDpiAwareness.restype = ctypes.c_long
        if shcore.SetProcessDpiAwareness(2) in (0, 1):
            return
    except (AttributeError, OSError):
        pass
    try:
        ctypes.WinDLL("user32", use_last_error=True).SetProcessDPIAware()
    except (AttributeError, OSError) as exc:
        LOG.warning("Windows DPI setup unavailable; using Tk DPI: %s", exc)
