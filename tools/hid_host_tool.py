#!/usr/bin/env python3
"""
TOS Custom HID host tool.

Default device: VID 0x0483, PID 0x5750, Vendor Report ID 0x10.
Install dependency:
    python -m pip install hidapi

Run GUI:
    python tools/hid_host_tool.py

CLI examples:
    python tools/hid_host_tool.py --list
    python tools/hid_host_tool.py --send "hello from PC" --read
"""
from __future__ import annotations

import argparse
import binascii
import queue
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional

try:
    import hid  # type: ignore
except Exception as exc:  # pragma: no cover - user environment dependent
    hid = None
    HID_IMPORT_ERROR = exc
else:
    HID_IMPORT_ERROR = None

DEFAULT_VID = 0x0483
DEFAULT_PID = 0x5750
REPORT_ID_VENDOR = 0x10
REPORT_SIZE = 64
PAYLOAD_SIZE = REPORT_SIZE - 1


@dataclass
class HidEntry:
    path: bytes
    vendor_id: int
    product_id: int
    usage_page: int
    usage: int
    manufacturer: str
    product: str
    serial_number: str
    interface_number: int

    @classmethod
    def from_dict(cls, item: dict) -> "HidEntry":
        path = item.get("path", b"")
        if isinstance(path, str):
            path = path.encode(errors="ignore")
        return cls(
            path=path,
            vendor_id=int(item.get("vendor_id", 0) or 0),
            product_id=int(item.get("product_id", 0) or 0),
            usage_page=int(item.get("usage_page", 0) or 0),
            usage=int(item.get("usage", 0) or 0),
            manufacturer=str(item.get("manufacturer_string", "") or ""),
            product=str(item.get("product_string", "") or ""),
            serial_number=str(item.get("serial_number", "") or ""),
            interface_number=int(item.get("interface_number", -1) or -1),
        )

    def label(self) -> str:
        return (
            f"VID={self.vendor_id:04X} PID={self.product_id:04X} "
            f"UP={self.usage_page:04X} U={self.usage:04X} "
            f"IF={self.interface_number} {self.manufacturer} {self.product}"
        ).strip()


def require_hid() -> None:
    if hid is None:
        raise RuntimeError(
            "Python hidapi is not installed. Run: python -m pip install hidapi"
        ) from HID_IMPORT_ERROR


def enumerate_devices(vid: int = DEFAULT_VID, pid: int = DEFAULT_PID) -> list[HidEntry]:
    require_hid()
    return [HidEntry.from_dict(x) for x in hid.enumerate(vid, pid)]


def pick_vendor_device(vid: int = DEFAULT_VID, pid: int = DEFAULT_PID) -> HidEntry:
    devices = enumerate_devices(vid, pid)
    if not devices:
        raise RuntimeError(f"No HID device found for VID={vid:04X} PID={pid:04X}")

    # Windows often exposes one logical HID path per top-level collection.
    # Prefer the vendor-defined collection from the 0xFF00 usage page.
    for dev in devices:
        if dev.usage_page == 0xFF00:
            return dev
    return devices[0]


def open_device(entry: HidEntry):
    require_hid()
    dev = hid.device()
    dev.open_path(entry.path)
    dev.set_nonblocking(True)
    return dev


def make_vendor_report(payload: bytes) -> bytes:
    payload = payload[:PAYLOAD_SIZE]
    return bytes([REPORT_ID_VENDOR]) + payload.ljust(PAYLOAD_SIZE, b"\x00")


def parse_vendor_report(data: list[int] | bytes | bytearray) -> Optional[bytes]:
    if not data:
        return None
    raw = bytes(data)
    if raw[0] != REPORT_ID_VENDOR:
        return None
    return raw[1:].rstrip(b"\x00")


def cli_main(args: argparse.Namespace) -> int:
    if args.list:
        for idx, dev in enumerate(enumerate_devices(args.vid, args.pid)):
            print(f"[{idx}] {dev.label()}")
        return 0

    entry = pick_vendor_device(args.vid, args.pid)
    print(f"Opening: {entry.label()}")
    dev = open_device(entry)

    try:
        if args.send is not None:
            payload = args.send.encode("utf-8")
            report = make_vendor_report(payload)
            n = dev.write(report)
            print(f"Sent {n} bytes: {args.send!r}")

        if args.read:
            deadline = time.time() + args.timeout
            while time.time() < deadline:
                data = dev.read(REPORT_SIZE, timeout_ms=100)
                payload = parse_vendor_report(data)
                if payload is not None:
                    print("RX text:", payload.decode("utf-8", errors="replace"))
                    print("RX hex :", binascii.hexlify(payload).decode())
                    return 0
            print("Read timeout")
            return 2
    finally:
        dev.close()

    return 0


def gui_main() -> int:
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except Exception as exc:
        print(f"Tkinter unavailable: {exc}", file=sys.stderr)
        return 1

    q: queue.Queue[str] = queue.Queue()
    stop_event = threading.Event()
    dev_holder: dict[str, object] = {"dev": None, "entry": None}

    root = tk.Tk()
    root.title("TOS HID Tool")
    root.geometry("820x520")

    vid_var = tk.StringVar(value=f"{DEFAULT_VID:04X}")
    pid_var = tk.StringVar(value=f"{DEFAULT_PID:04X}")
    send_var = tk.StringVar(value="hello from PC")
    device_var = tk.StringVar(value="")

    devices: list[HidEntry] = []

    top = ttk.Frame(root, padding=8)
    top.pack(fill=tk.X)

    ttk.Label(top, text="VID").pack(side=tk.LEFT)
    ttk.Entry(top, width=8, textvariable=vid_var).pack(side=tk.LEFT, padx=(4, 10))
    ttk.Label(top, text="PID").pack(side=tk.LEFT)
    ttk.Entry(top, width=8, textvariable=pid_var).pack(side=tk.LEFT, padx=(4, 10))

    combo = ttk.Combobox(top, textvariable=device_var, state="readonly", width=72)
    combo.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))

    log = tk.Text(root, height=18)
    log.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)

    bottom = ttk.Frame(root, padding=8)
    bottom.pack(fill=tk.X)
    ttk.Entry(bottom, textvariable=send_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))

    def append(text: str) -> None:
        log.insert(tk.END, text + "\n")
        log.see(tk.END)

    def get_vid_pid() -> tuple[int, int]:
        return int(vid_var.get(), 16), int(pid_var.get(), 16)

    def refresh() -> None:
        nonlocal devices
        try:
            vid, pid = get_vid_pid()
            devices = enumerate_devices(vid, pid)
            labels = [d.label() for d in devices]
            combo["values"] = labels
            if labels:
                # Prefer vendor collection.
                sel = 0
                for i, d in enumerate(devices):
                    if d.usage_page == 0xFF00:
                        sel = i
                        break
                combo.current(sel)
                append(f"Found {len(labels)} HID collection(s).")
            else:
                device_var.set("")
                append("No device found.")
        except Exception as exc:
            messagebox.showerror("Refresh failed", str(exc))

    def close_dev() -> None:
        dev = dev_holder.get("dev")
        if dev is not None:
            try:
                dev.close()
            except Exception:
                pass
        dev_holder["dev"] = None
        dev_holder["entry"] = None

    def read_worker(dev) -> None:
        while not stop_event.is_set():
            try:
                data = dev.read(REPORT_SIZE, timeout_ms=100)
                payload = parse_vendor_report(data)
                if payload is not None:
                    text = payload.decode("utf-8", errors="replace")
                    hx = binascii.hexlify(payload).decode()
                    q.put(f"RX text: {text}\nRX hex : {hx}")
            except Exception as exc:
                q.put(f"Read stopped: {exc}")
                break

    def connect() -> None:
        close_dev()
        stop_event.clear()
        idx = combo.current()
        if idx < 0 or idx >= len(devices):
            messagebox.showwarning("Connect", "Please refresh and select a device first.")
            return
        try:
            entry = devices[idx]
            dev = open_device(entry)
            dev_holder["dev"] = dev
            dev_holder["entry"] = entry
            append(f"Connected: {entry.label()}")
            threading.Thread(target=read_worker, args=(dev,), daemon=True).start()
        except Exception as exc:
            messagebox.showerror("Connect failed", str(exc))

    def send() -> None:
        dev = dev_holder.get("dev")
        if dev is None:
            messagebox.showwarning("Send", "Connect first.")
            return
        try:
            payload = send_var.get().encode("utf-8")
            report = make_vendor_report(payload)
            n = dev.write(report)
            append(f"TX {n} bytes: {send_var.get()!r}")
        except Exception as exc:
            messagebox.showerror("Send failed", str(exc))

    def pump_queue() -> None:
        try:
            while True:
                append(q.get_nowait())
        except queue.Empty:
            pass
        root.after(100, pump_queue)

    def on_close() -> None:
        stop_event.set()
        close_dev()
        root.destroy()

    ttk.Button(top, text="Refresh", command=refresh).pack(side=tk.LEFT, padx=(0, 4))
    ttk.Button(top, text="Connect", command=connect).pack(side=tk.LEFT)
    ttk.Button(bottom, text="Send Vendor OUT", command=send).pack(side=tk.LEFT)

    append("Install dependency: python -m pip install hidapi")
    append("Use Report ID 0x10 vendor channel: 1-byte ID + 63-byte payload.")
    root.protocol("WM_DELETE_WINDOW", on_close)
    root.after(100, pump_queue)
    refresh()
    root.mainloop()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="TOS Custom HID host tool")
    parser.add_argument("--vid", type=lambda x: int(x, 16), default=DEFAULT_VID)
    parser.add_argument("--pid", type=lambda x: int(x, 16), default=DEFAULT_PID)
    parser.add_argument("--list", action="store_true", help="list matching HID collections")
    parser.add_argument("--send", help="send text to device through vendor OUT report")
    parser.add_argument("--read", action="store_true", help="read one vendor IN report")
    parser.add_argument("--timeout", type=float, default=5.0, help="read timeout seconds")
    parser.add_argument("--gui", action="store_true", help="force GUI mode")
    args = parser.parse_args()

    try:
        if args.gui or (not args.list and args.send is None and not args.read):
            return gui_main()
        return cli_main(args)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
