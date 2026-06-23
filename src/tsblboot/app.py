#!/usr/bin/env python3
"""TSBL Boot host tool for the TOS SBL USB CDC protocol."""

import argparse
import pathlib
import sys
import time
import zlib

try:
    from . import __version__
except ImportError:  # direct script execution
    __version__ = "1.0.0"


VID = 0x0483
PID = 0x5751
DEFAULT_CHUNK = 16 * 1024
PARTITIONS = {
    "sbl": {
        "size": 32 * 1024,
        "flash": True,
        "erase": False,
        "staged": True,
        "note": "staged through TMP, then ELF applies it after reboot",
    },
    "rec": {
        "size": 64 * 1024,
        "flash": True,
        "erase": True,
        "staged": True,
        "note": "staged flash; direct erase is allowed for recovery testing",
    },
    "sah": {
        "size": 128 * 1024,
        "flash": True,
        "erase": True,
        "staged": False,
        "note": "direct flash",
    },
    "system": {
        "size": 512 * 1024,
        "flash": True,
        "erase": True,
        "staged": False,
        "note": "direct flash",
    },
    "tmp": {
        "size": 128 * 1024,
        "flash": False,
        "erase": True,
        "staged": False,
        "note": "erase-only temporary staging partition",
    },
    "userdata": {
        "size": 128 * 1024,
        "flash": False,
        "erase": True,
        "staged": False,
        "note": "erase-only persistent settings/userdata partition",
    },
}
VALID_FLASH_PARTITIONS = tuple(name for name, meta in PARTITIONS.items() if meta["flash"])
VALID_ERASE_PARTITIONS = tuple(name for name, meta in PARTITIONS.items() if meta["erase"])


def import_serial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("pyserial is required: python -m pip install pyserial", file=sys.stderr)
        raise SystemExit(2)
    return serial, list_ports


def find_port(list_ports):
    for port in list_ports.comports():
        text = " ".join(
            str(x or "")
            for x in (port.description, port.manufacturer, port.product, port.hwid)
        ).lower()
        if port.vid == VID and port.pid == PID:
            return port.device
        if "tos sbl" in text or "fastboot" in text:
            return port.device
    return None


def open_serial_with_retry(serial, port, baud, seconds=3.0):
    deadline = time.monotonic() + seconds
    last_error = None
    while time.monotonic() < deadline:
        try:
            return serial.Serial(port, baud, timeout=0.05, write_timeout=3.0)
        except (serial.SerialException, OSError, PermissionError) as exc:
            last_error = exc
            time.sleep(0.25)
    print(f"Cannot open {port}: {last_error}", file=sys.stderr)
    print("If the device just rebooted, wait a moment or replug USB, then retry.", file=sys.stderr)
    return None


def read_available(ser, seconds=1.0):
    deadline = time.monotonic() + seconds
    chunks = []
    while time.monotonic() < deadline:
        try:
            waiting = ser.in_waiting
            if waiting:
                chunks.append(ser.read(waiting))
                deadline = time.monotonic() + 0.15
            else:
                time.sleep(0.02)
        except Exception as exc:
            print(f"\nSerial read failed: {exc}", file=sys.stderr)
            break
    return b"".join(chunks)


def read_line(ser, timeout=3.0, echo=True):
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        try:
            chunk = ser.read(1)
        except Exception as exc:
            raise RuntimeError(f"Serial read failed: {exc}") from exc
        if chunk:
            buf += chunk
            if chunk == b"\n":
                text = buf.decode(errors="replace")
                if echo:
                    print(text, end="", flush=True)
                return text.strip()
        else:
            time.sleep(0.01)
    raise TimeoutError("Timed out waiting for device response.")


def expect_okay_or_fail(ser, timeout=5.0, echo=True):
    while True:
        line = read_line(ser, timeout=timeout, echo=echo)
        if not line:
            continue
        if line.startswith("OKAY") or line.startswith("FAIL"):
            return line


def send_ascii_command(ser, payload, timeout=3.0, expect_final=False):
    ser.write(payload.encode("ascii"))
    ser.flush()
    if expect_final:
        return expect_okay_or_fail(ser, timeout=timeout)
    return read_available(ser, timeout).decode(errors="replace")


def crc32_bytes(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def partition_list(names):
    return ", ".join(names)


def parse_ready_chunk(line):
    parts = line.split()
    if len(parts) >= 3 and parts[0] == "OKAY" and parts[1] == "READY":
        try:
            return int(parts[2], 10)
        except ValueError:
            pass
    return DEFAULT_CHUNK


def read_file(path_str):
    path = pathlib.Path(path_str)
    try:
      data = path.read_bytes()
    except OSError as exc:
      raise RuntimeError(f"Cannot read {path}: {exc}") from exc
    return path, data


def do_flash(ser, part_name, image_path):
    part_name = part_name.lower()
    if part_name not in VALID_FLASH_PARTITIONS:
        raise RuntimeError(
            f"Unsupported flash partition '{part_name}'. "
            f"Valid: {partition_list(VALID_FLASH_PARTITIONS)}"
        )
    path, data = read_file(image_path)
    if not data:
        raise RuntimeError(f"{path} is empty.")
    if len(data) & 3:
        raise RuntimeError(f"{path} size must be 4-byte aligned for STM32 flash.")
    expected_size = PARTITIONS[part_name]["size"]
    if len(data) != expected_size:
        raise RuntimeError(
            f"{path} size mismatch for {part_name}: "
            f"expected {expected_size} bytes, got {len(data)} bytes."
        )

    image_crc = crc32_bytes(data)
    begin = f"FLASHBEGIN {part_name} {len(data)} 0x{image_crc:08X}\n"
    ser.write(begin.encode("ascii"))
    ser.flush()
    begin_rsp = expect_okay_or_fail(ser, timeout=5.0)
    if begin_rsp.startswith("FAIL"):
        print(begin_rsp)
        return 1

    chunk_size = parse_ready_chunk(begin_rsp)
    print(
        f"INFO flashing {part_name} from {path} "
        f"({len(data)} bytes, crc=0x{image_crc:08X}, chunk={chunk_size})"
    )
    print(f"INFO mode: {PARTITIONS[part_name]['note']}")

    for offset in range(0, len(data), chunk_size):
        chunk = data[offset:offset + chunk_size]
        chunk_crc = crc32_bytes(chunk)
        header = f"FLASHDATA {len(chunk)} {offset} 0x{chunk_crc:08X}\n"
        ser.write(header.encode("ascii"))
        ser.flush()
        rsp = expect_okay_or_fail(ser, timeout=5.0)
        if rsp.startswith("FAIL"):
            print(rsp)
            return 1
        if rsp != "OKAY SEND":
            raise RuntimeError(f"Unexpected device response before chunk data: {rsp}")

        ser.write(chunk)
        ser.flush()
        rsp = expect_okay_or_fail(ser, timeout=20.0)
        if rsp.startswith("FAIL"):
            print(rsp)
            return 1
        percent = ((offset + len(chunk)) * 100) // len(data)
        print(f"INFO chunk {offset:>7}/{len(data)} -> {percent:>3}%")

    ser.write(b"FLASHEND\n")
    ser.flush()
    end_timeout = 35.0 if PARTITIONS[part_name]["staged"] else 20.0
    end_rsp = expect_okay_or_fail(ser, timeout=end_timeout)
    print(end_rsp)
    if end_rsp.startswith("OKAY STAGED"):
        print("INFO staged update accepted; device is rebooting to let ELF apply it.")
        print("INFO wait until FASTBOOT reappears, then run: tsblboot info")
    return 0 if end_rsp.startswith("OKAY") else 1


def do_wait_fastboot(serial, list_ports, port, baud, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        candidate = port or find_port(list_ports)
        if candidate:
            ser = open_serial_with_retry(serial, candidate, baud, seconds=1.0)
            if ser:
                with ser:
                    time.sleep(0.25)
                    banner = read_available(ser, 0.8).decode(errors="replace")
                    if banner:
                        print(banner, end="")
                    info = send_ascii_command(ser, "INFO\n", timeout=1.5)
                    if info:
                        print(info, end="")
                    return 0
        time.sleep(0.5)
    print("Timed out waiting for FASTBOOT CDC to reappear.", file=sys.stderr)
    return 1


def print_examples():
    fw = r"C:\Code\tos\slave-board\dist\firmware"
    tool = r"tsblboot"
    print("List ports:")
    print(f"  {tool} ports")
    print("Check FASTBOOT info:")
    print(f"  {tool} --port COM8 info")
    print("Reboot targets:")
    print(f"  {tool} --port COM8 reboot")
    print(f"  {tool} --port COM8 reboot recovery")
    print("Unlock before flashing or erasing:")
    print(f"  {tool} --port COM8 oem unlock")
    print("Flash staged boot partitions:")
    print(f"  {tool} --port COM8 --wait flash sbl {fw}\\sbl.bin")
    print(f"  {tool} --port COM8 --wait flash rec {fw}\\rec.bin")
    print("Flash direct partitions:")
    print(f"  {tool} --port COM8 flash sah {fw}\\sah.bin")
    print(f"  {tool} --port COM8 flash system {fw}\\system.bin")
    print("Erase direct partitions:")
    print(f"  {tool} --port COM8 erase rec")
    print(f"  {tool} --port COM8 erase sah")
    print(f"  {tool} --port COM8 erase system")
    print(f"  {tool} --port COM8 erase tmp")
    print(f"  {tool} --port COM8 erase userdata")
    print("Raw command example:")
    print(f"  {tool} --port COM8 raw INFO")


def main():
    parser = argparse.ArgumentParser(prog="tsblboot", description="Talk to TOS SBL FASTBOOT CDC.")
    parser.add_argument("--version", action="version", version=f"tsblboot {__version__}")
    parser.add_argument("-p", "--port", help="COM port, e.g. COM8")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument(
        "--wait",
        action="store_true",
        help="after a staged flash, wait for FASTBOOT CDC to reappear",
    )
    parser.add_argument(
        "--wait-timeout",
        type=float,
        default=20.0,
        help="seconds to wait for FASTBOOT after staged flashes",
    )
    parser.add_argument(
        "words",
        nargs="*",
        help=(
            "ports, info, partitions, reboot, reboot recovery, "
            "oem unlock, oem lock, "
            f"flash <{partition_list(VALID_FLASH_PARTITIONS).replace(', ', '|')}> <bin>, "
            f"erase <{partition_list(VALID_ERASE_PARTITIONS).replace(', ', '|')}>, "
            "examples, raw <command>"
        ),
    )
    args = parser.parse_args()

    words = args.words or ["info"]
    command = " ".join(words).strip().lower()

    if command == "partitions":
        for name, meta in PARTITIONS.items():
            flags = []
            if meta["flash"]:
                flags.append("flash")
            if meta["erase"]:
                flags.append("erase")
            if meta["staged"]:
                flags.append("staged")
            print(f"{name:7} size={meta['size']:>6} flags={','.join(flags):18} {meta['note']}")
        return 0

    if command == "examples":
        print_examples()
        return 0

    serial, list_ports = import_serial()

    if command == "ports":
        for port in list_ports.comports():
            print(f"{port.device:8} vid={port.vid!s:>6} pid={port.pid!s:>6} {port.description}")
        return 0

    port = args.port or find_port(list_ports)
    if not port:
        print("No TOS SBL CDC port found. Use --port COMx.", file=sys.stderr)
        return 1

    ser = open_serial_with_retry(serial, port, args.baud)
    if ser is None:
        return 1

    with ser:
        try:
            ser.reset_input_buffer()
            time.sleep(0.25)
            banner = read_available(ser, 0.6)
            if banner:
                print(banner.decode(errors="replace"), end="")

            if words[0].lower() == "flash":
                if len(words) != 3:
                    print(
                        "Usage: tsblboot --port COM8 flash "
                        f"<{partition_list(VALID_FLASH_PARTITIONS).replace(', ', '|')}> <bin>",
                        file=sys.stderr,
                    )
                    return 2
                rc = do_flash(ser, words[1], words[2])
                if rc == 0 and args.wait and PARTITIONS[words[1].lower()]["staged"]:
                    return do_wait_fastboot(serial, list_ports, port, args.baud, args.wait_timeout)
                return rc
            if words[0].lower() == "erase":
                if len(words) != 2:
                    print(
                        "Usage: tsblboot --port COM8 erase "
                        f"<{partition_list(VALID_ERASE_PARTITIONS).replace(', ', '|')}>",
                        file=sys.stderr,
                    )
                    return 2
                part = words[1].lower()
                if part not in VALID_ERASE_PARTITIONS:
                    print(
                        f"Unsupported erase partition. Valid: {partition_list(VALID_ERASE_PARTITIONS)}",
                        file=sys.stderr,
                    )
                    return 2
                erase_timeout = 90.0 if part in ("system", "tmp", "userdata") else 25.0
                rsp = send_ascii_command(ser, f"ERASE {part}\n", timeout=erase_timeout, expect_final=True)
                return 0 if rsp.startswith("OKAY") else 1

            if command == "info":
                print(send_ascii_command(ser, "INFO\n", timeout=1.5), end="")
                return 0
            if command == "reboot":
                rsp = send_ascii_command(ser, "REBOOT\n", timeout=2.0, expect_final=False)
                print(rsp, end="")
                return 0
            if command in ("reboot recovery", "reboot-recovery"):
                rsp = send_ascii_command(
                    ser, "REBOOT RECOVERY\n", timeout=2.0, expect_final=False
                )
                print(rsp, end="")
                return 0
            if command == "oem unlock":
                rsp = send_ascii_command(ser, "OEM UNLOCK\n", timeout=35.0, expect_final=True)
                return 0 if rsp.startswith("OKAY") else 1
            if command == "oem lock":
                rsp = send_ascii_command(ser, "OEM LOCK\n", timeout=5.0, expect_final=True)
                return 0 if rsp.startswith("OKAY") else 1
            if command.startswith("raw "):
                print(send_ascii_command(ser, command[4:].upper() + "\n", timeout=3.0), end="")
                return 0

            print(f"Unknown command: {' '.join(words)}", file=sys.stderr)
            return 2
        except TimeoutError as exc:
            print(str(exc), file=sys.stderr)
            return 1
        except RuntimeError as exc:
            print(str(exc), file=sys.stderr)
            return 1
        except Exception as exc:
            print(f"Unexpected failure: {exc}", file=sys.stderr)
            return 1


if __name__ == "__main__":
    raise SystemExit(main())
