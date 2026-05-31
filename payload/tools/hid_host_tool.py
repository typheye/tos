#!/usr/bin/env python3
"""
TOS Custom HID host tool (improved GUI).

Default device: VID 0x0483, PID 0x5750, Vendor Report ID 0x10.
Install dependency:
    python -m pip install hidapi

Run GUI:
    python tools/hid_host_tool.py

CLI examples:
    python tools/hid_host_tool.py --list
    python tools/hid_host_tool.py --send "hello from PC"
    python tools/hid_host_tool.py --read
"""
from __future__ import annotations

import argparse
import binascii
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Optional

try:
    import hid  # type: ignore
except Exception as exc:  # pragma: no cover
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

    def kind(self) -> str:
        if self.usage_page == 0xFF00:
            return "Vendor"
        if self.usage_page == 0x0001 and self.usage == 0x0006:
            return "Keyboard"
        if self.usage_page == 0x0001 and self.usage == 0x0002:
            return "Mouse"
        return "Other"

    def short_label(self) -> str:
        name = (self.product or "HID").strip()
        return f"{self.kind():<8} UP={self.usage_page:04X} U={self.usage:04X}  {name}"

    def label(self) -> str:
        return (
            f"{self.kind()}  VID={self.vendor_id:04X} PID={self.product_id:04X} "
            f"UP={self.usage_page:04X} U={self.usage:04X} IF={self.interface_number} "
            f"{self.manufacturer} {self.product}"
        ).strip()


def require_hid() -> None:
    if hid is None:
        raise RuntimeError(
            "Python hidapi is not installed. Run: python -m pip install hidapi"
        ) from HID_IMPORT_ERROR


def enumerate_devices(vid: int = DEFAULT_VID, pid: int = DEFAULT_PID) -> list[HidEntry]:
    require_hid()
    return [HidEntry.from_dict(x) for x in hid.enumerate(vid, pid)]


def pick_vendor_device(devices: list[HidEntry]) -> Optional[HidEntry]:
    for dev in devices:
        if dev.usage_page == 0xFF00:
            return dev
    return devices[0] if devices else None


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
    devices = enumerate_devices(args.vid, args.pid)
    if args.list:
        for idx, dev in enumerate(devices):
            print(f"[{idx}] {dev.label()}")
        return 0

    entry = pick_vendor_device(devices)
    if entry is None:
        raise RuntimeError(f"No HID device found for VID={args.vid:04X} PID={args.pid:04X}")

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
                data = dev.read(REPORT_SIZE, timeout_ms=120)
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

    q: queue.Queue[tuple[str, str]] = queue.Queue()
    stop_event = threading.Event()

    root = tk.Tk()
    root.title("TOS HID Tool")
    root.geometry("980x660")
    root.minsize(900, 600)

    style = ttk.Style(root)
    try:
        style.theme_use("vista")
    except Exception:
        pass

    vid_var = tk.StringVar(value=f"{DEFAULT_VID:04X}")
    pid_var = tk.StringVar(value=f"{DEFAULT_PID:04X}")
    send_var = tk.StringVar(value="hello from PC")
    device_var = tk.StringVar(value="")
    status_var = tk.StringVar(value="Disconnected")
    hint_var = tk.StringVar(value="Refresh to scan HID collections. The tool prefers the Vendor collection (UP=FF00).")
    autoconnect_var = tk.BooleanVar(value=True)
    monitor_var = tk.BooleanVar(value=True)

    devices: list[HidEntry] = []
    dev_holder: dict[str, object] = {"dev": None, "entry": None, "thread": None}
    last_selected_path: list[bytes | None] = [None]

    top = ttk.Frame(root, padding=10)
    top.pack(fill=tk.X)

    ttk.Label(top, text="VID").grid(row=0, column=0, sticky="w")
    ttk.Entry(top, width=8, textvariable=vid_var).grid(row=0, column=1, padx=(4, 10), sticky="w")
    ttk.Label(top, text="PID").grid(row=0, column=2, sticky="w")
    ttk.Entry(top, width=8, textvariable=pid_var).grid(row=0, column=3, padx=(4, 10), sticky="w")

    ttk.Label(top, text="Collection").grid(row=1, column=0, sticky="w", pady=(8, 0))
    combo = ttk.Combobox(top, textvariable=device_var, state="readonly", width=92)
    combo.grid(row=1, column=1, columnspan=5, sticky="ew", pady=(8, 0), padx=(4, 8))

    button_bar = ttk.Frame(top)
    button_bar.grid(row=0, column=6, rowspan=2, sticky="e")
    btn_refresh = ttk.Button(button_bar, text="Refresh")
    btn_connect = ttk.Button(button_bar, text="Connect")
    btn_disconnect = ttk.Button(button_bar, text="Disconnect")
    btn_refresh.pack(side=tk.LEFT, padx=(0, 6))
    btn_connect.pack(side=tk.LEFT, padx=(0, 6))
    btn_disconnect.pack(side=tk.LEFT)
    top.columnconfigure(5, weight=1)

    opts = ttk.Frame(root, padding=(10, 0, 10, 6))
    opts.pack(fill=tk.X)
    ttk.Checkbutton(opts, text="Auto connect preferred Vendor collection", variable=autoconnect_var).pack(side=tk.LEFT)
    ttk.Checkbutton(opts, text="Monitor incoming Vendor IN reports", variable=monitor_var).pack(side=tk.LEFT, padx=(18, 0))

    status_frame = ttk.LabelFrame(root, text="Connection status", padding=10)
    status_frame.pack(fill=tk.X, padx=10, pady=(0, 8))
    ttk.Label(status_frame, textvariable=status_var, font=("Segoe UI", 10, "bold")).pack(anchor="w")
    ttk.Label(status_frame, textvariable=hint_var, wraplength=920, foreground="#555555").pack(anchor="w", pady=(4, 0))

    mid = ttk.Panedwindow(root, orient="horizontal")
    mid.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

    left = ttk.Frame(mid)
    right = ttk.Frame(mid, width=280)
    mid.add(left, weight=3)
    mid.add(right, weight=1)

    log_frame = ttk.LabelFrame(left, text="Log", padding=8)
    log_frame.pack(fill=tk.BOTH, expand=True)
    log = tk.Text(log_frame, wrap="word", height=22)
    log_scroll = ttk.Scrollbar(log_frame, orient="vertical", command=log.yview)
    log.configure(yscrollcommand=log_scroll.set)
    log.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
    log_scroll.pack(side=tk.RIGHT, fill=tk.Y)

    right_top = ttk.LabelFrame(right, text="Quick actions", padding=8)
    right_top.pack(fill=tk.X)
    ttk.Button(right_top, text="Ping / Hello", command=lambda: send_text("hello from PC"), width=24).pack(fill=tk.X, pady=2)
    ttk.Button(right_top, text="Read once", command=lambda: read_once(), width=24).pack(fill=tk.X, pady=2)
    ttk.Button(right_top, text="Clear log", command=lambda: clear_log(), width=24).pack(fill=tk.X, pady=2)

    info_frame = ttk.LabelFrame(right, text="Tips", padding=8)
    info_frame.pack(fill=tk.BOTH, expand=True, pady=(8, 0))
    info_text = (
        "• This device exposes 3 HID collections: Keyboard, Mouse, Vendor.\n"
        "• The PC tool should connect to the Vendor collection (Usage Page FF00).\n"
        "• Device-side menu: TOS -> 03 HID Test.\n"
        "• To verify PC -> MCU: send text here, then on the device press '03 Read PC OUT'.\n"
        "• To verify MCU -> PC: on the device press '02 Vendor Hello'."
    )
    ttk.Label(info_frame, text=info_text, wraplength=250, justify="left").pack(anchor="w")

    send_frame = ttk.LabelFrame(root, text="Vendor OUT -> Device", padding=10)
    send_frame.pack(fill=tk.X, padx=10, pady=(0, 10))
    send_entry = ttk.Entry(send_frame, textvariable=send_var)
    send_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))
    btn_send = ttk.Button(send_frame, text="Send text")
    btn_send.pack(side=tk.LEFT)

    def now() -> str:
        return datetime.now().strftime("%H:%M:%S")

    def append(text: str, tag: str = "INFO") -> None:
        log.insert(tk.END, f"[{now()}] {tag:<5} {text}\n")
        log.see(tk.END)

    def clear_log() -> None:
        log.delete("1.0", tk.END)

    def get_vid_pid() -> tuple[int, int]:
        return int(vid_var.get(), 16), int(pid_var.get(), 16)

    def set_connected_ui(connected: bool) -> None:
        if connected:
            status_var.set("Connected")
            btn_send.state(["!disabled"])
            btn_disconnect.state(["!disabled"])
        else:
            status_var.set("Disconnected")
            btn_send.state(["disabled"])
            btn_disconnect.state(["disabled"])

    def close_dev() -> None:
        stop_event.set()
        dev = dev_holder.get("dev")
        if dev is not None:
            try:
                dev.close()
            except Exception:
                pass
        dev_holder["dev"] = None
        dev_holder["entry"] = None
        dev_holder["thread"] = None
        set_connected_ui(False)

    def read_worker(dev) -> None:
        while not stop_event.is_set():
            if not monitor_var.get():
                time.sleep(0.1)
                continue
            try:
                data = dev.read(REPORT_SIZE, timeout_ms=100)
                payload = parse_vendor_report(data)
                if payload is not None:
                    text = payload.decode("utf-8", errors="replace")
                    hx = binascii.hexlify(payload).decode()
                    q.put(("RX", f"Vendor IN text: {text}"))
                    q.put(("RX", f"Vendor IN hex : {hx}"))
            except Exception as exc:
                q.put(("ERR", f"Read stopped: {exc}"))
                break

    def selected_entry() -> Optional[HidEntry]:
        idx = combo.current()
        if 0 <= idx < len(devices):
            return devices[idx]
        return None

    def choose_preferred_index() -> int:
        if last_selected_path[0] is not None:
            for i, dev in enumerate(devices):
                if dev.path == last_selected_path[0]:
                    return i
        for i, dev in enumerate(devices):
            if dev.usage_page == 0xFF00:
                return i
        return 0

    def connect_selected(auto: bool = False) -> bool:
        stop_event.clear()
        entry = selected_entry()
        if entry is None:
            if not auto:
                messagebox.showwarning("Connect", "Please refresh and select a device first.")
            return False
        try:
            close_dev()
            stop_event.clear()
            dev = open_device(entry)
            dev_holder["dev"] = dev
            dev_holder["entry"] = entry
            dev_holder["thread"] = threading.Thread(target=read_worker, args=(dev,), daemon=True)
            dev_holder["thread"].start()
            last_selected_path[0] = entry.path
            set_connected_ui(True)
            status_var.set(f"Connected to {entry.kind()} collection")
            hint_var.set(f"Path opened successfully. Usage Page={entry.usage_page:04X}, Usage={entry.usage:04X}. You can send Vendor OUT text below.")
            append(f"Connected: {entry.label()}")
            return True
        except Exception as exc:
            set_connected_ui(False)
            msg = f"Connect failed: {exc}"
            append(msg, "ERR")
            if not auto:
                messagebox.showerror("Connect failed", str(exc))
            return False

    def refresh(auto_connect: Optional[bool] = None) -> None:
        nonlocal devices
        try:
            vid, pid = get_vid_pid()
            devices = enumerate_devices(vid, pid)
            labels = [d.short_label() for d in devices]
            combo["values"] = labels
            if labels:
                sel = choose_preferred_index()
                combo.current(sel)
                kinds = ", ".join([d.kind() for d in devices])
                append(f"Found {len(labels)} HID collection(s): {kinds}")
                hint_var.set("Keyboard/Mouse collections are for input emulation; Vendor collection (FF00) is used for PC <-> MCU custom data.")
                if auto_connect is None:
                    auto_connect = autoconnect_var.get()
                if auto_connect:
                    connect_selected(auto=True)
            else:
                device_var.set("")
                close_dev()
                append("No matching HID device found.", "WARN")
                hint_var.set("No device found. Check USB cable, VID/PID, or whether the board has enumerated.")
        except Exception as exc:
            close_dev()
            messagebox.showerror("Refresh failed", str(exc))

    def send_text(text: Optional[str] = None) -> None:
        dev = dev_holder.get("dev")
        if dev is None:
            if autoconnect_var.get():
                refresh(auto_connect=True)
                dev = dev_holder.get("dev")
            if dev is None:
                messagebox.showwarning("Send", "No active connection. Click Refresh/Connect first.")
                return
        try:
            content = send_var.get() if text is None else text
            payload = content.encode("utf-8")
            report = make_vendor_report(payload)
            n = dev.write(report)
            append(f"Vendor OUT sent ({n} bytes): {content}", "TX")
        except Exception as exc:
            append(f"Send failed: {exc}", "ERR")
            messagebox.showerror("Send failed", str(exc))

    def read_once() -> None:
        dev = dev_holder.get("dev")
        if dev is None:
            messagebox.showwarning("Read", "No active connection.")
            return
        try:
            data = dev.read(REPORT_SIZE, timeout_ms=200)
            payload = parse_vendor_report(data)
            if payload is None:
                append("No Vendor IN report received this time.", "INFO")
                return
            append(f"Vendor IN text: {payload.decode('utf-8', errors='replace')}", "RX")
            append(f"Vendor IN hex : {binascii.hexlify(payload).decode()}", "RX")
        except Exception as exc:
            append(f"Read failed: {exc}", "ERR")
            messagebox.showerror("Read failed", str(exc))

    def pump_queue() -> None:
        try:
            while True:
                tag, text = q.get_nowait()
                append(text, tag)
        except queue.Empty:
            pass
        root.after(100, pump_queue)

    def on_close() -> None:
        close_dev()
        root.destroy()

    def on_select(_event=None) -> None:
        entry = selected_entry()
        if entry is not None:
            hint_var.set(
                f"Selected {entry.kind()} collection. For custom data, connect to Vendor (Usage Page FF00)."
            )

    btn_refresh.configure(command=lambda: refresh())
    btn_connect.configure(command=lambda: connect_selected(auto=False))
    btn_disconnect.configure(command=close_dev)
    btn_send.configure(command=lambda: send_text())
    combo.bind("<<ComboboxSelected>>", on_select)

    set_connected_ui(False)
    append("Install dependency if needed: python -m pip install hidapi")
    append("This board exposes 3 collections: Keyboard, Mouse, Vendor.")
    append("The tool will prefer the Vendor collection (Usage Page FF00).")
    append("Device-side menu: TOS -> 03 HID Test")
    root.protocol("WM_DELETE_WINDOW", on_close)
    root.after(100, pump_queue)
    refresh(auto_connect=True)
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
