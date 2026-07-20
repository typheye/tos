#!/usr/bin/env python3
"""
 ******************************************************************************
 * @file    sbltool/app.py
 * @author  Typheye
 * @brief   SBL FASTBOOT CDC host implementation.
 ******************************************************************************
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 ******************************************************************************
 """

import argparse
import pathlib
import re
import sys
import time
import zlib

__version__ = "1.0.2"


VID = 0x0483
PID = 0x5751
DEFAULT_CHUNK = 16 * 1024


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


def getvar(ser, name):
    ser.reset_input_buffer()
    ser.write(f"GETVAR {name}\n".encode("ascii"))
    ser.flush()
    response = expect_okay_or_fail(ser, timeout=3.0, echo=False)
    if response.startswith("FAIL"):
        raise RuntimeError(response[4:] or f"unknown variable: {name}")
    return response[4:].strip()


def query_partitions(ser):
    ser.reset_input_buffer()
    ser.write(b"OEM PARTITIONS\n")
    ser.flush()
    result = []
    while True:
        line = read_line(ser, timeout=3.0, echo=False)
        if line.startswith("INFOpartition:"):
            fields = line[4:].split(":")
            item = {fields[i]: fields[i + 1] for i in range(0, len(fields) - 1, 2)}
            result.append(item)
        elif line.startswith("OKAY"):
            return result
        elif line.startswith("FAIL"):
            raise RuntimeError(line[4:] or "partition query failed")


def crc32_bytes(data):
    return zlib.crc32(data) & 0xFFFFFFFF


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
    path, data = read_file(image_path)
    started = time.monotonic()
    if not data:
        raise RuntimeError(f"{path} is empty.")
    if len(data) & 3:
        raise RuntimeError(f"{path} size must be 4-byte aligned for STM32 flash.")
    expected_size = int(getvar(ser, f"partition-size:{part_name}"), 0)
    if len(data) != expected_size:
        raise RuntimeError(
            f"{path} size mismatch for {part_name}: "
            f"expected {expected_size} bytes, got {len(data)} bytes."
        )

    image_crc = crc32_bytes(data)
    begin = f"FLASHBEGIN {part_name} {len(data)} 0x{image_crc:08X}\n"
    ser.write(begin.encode("ascii"))
    ser.flush()
    begin_rsp = expect_okay_or_fail(ser, timeout=120.0, echo=False)
    if begin_rsp.startswith("FAIL"):
        print(begin_rsp)
        return 1

    chunk_size = parse_ready_chunk(begin_rsp)
    print(f"Sending '{part_name}' ({len(data) // 1024} KB)".ljust(52), end="", flush=True)

    for offset in range(0, len(data), chunk_size):
        chunk = data[offset:offset + chunk_size]
        chunk_crc = crc32_bytes(chunk)
        header = f"FLASHDATA {len(chunk)} {offset} 0x{chunk_crc:08X}\n"
        ser.write(header.encode("ascii"))
        ser.flush()
        rsp = expect_okay_or_fail(ser, timeout=10.0, echo=False)
        if rsp.startswith("FAIL"):
            print(rsp)
            return 1
        if rsp != "OKAY SEND":
            raise RuntimeError(f"Unexpected device response before chunk data: {rsp}")

        ser.write(chunk)
        ser.flush()
        rsp = expect_okay_or_fail(ser, timeout=20.0, echo=False)
        if rsp.startswith("FAIL"):
            print(rsp)
            return 1
        percent = ((offset + len(chunk)) * 100) // len(data)
        print(f"\rSending '{part_name}' ({percent:3d}%)".ljust(52), end="", flush=True)

    ser.write(b"FLASHEND\n")
    ser.flush()
    end_timeout = 35.0
    end_rsp = expect_okay_or_fail(ser, timeout=end_timeout, echo=False)
    elapsed = time.monotonic() - started
    print(f"\rSending '{part_name}' ({len(data) // 1024} KB)".ljust(52) + f"OKAY [{elapsed:7.3f}s]")
    if end_rsp.startswith("OKAY STAGED"):
        print(f"Writing '{part_name}'".ljust(52) + "OKAY [ staged ]")
    elif end_rsp.startswith("OKAY"):
        print(f"Writing '{part_name}'".ljust(52) + "OKAY")
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
                    info = send_ascii_command(ser, "GETVAR serialno\n", timeout=1.5)
                    if info:
                        print(info, end="")
                    return 0
        time.sleep(0.5)
    print("Timed out waiting for FASTBOOT CDC to reappear.", file=sys.stderr)
    return 1


def print_examples():
    fw = r"C:\Code\tos\slave-board\dist\firmware"
    tool = r"sbltool"
    print("List ports:")
    print(f"  {tool} ports")
    print("Check FASTBOOT info:")
    print(f"  {tool} info")
    print("Reboot targets:")
    print(f"  {tool} reboot")
    print(f"  {tool} reboot recovery")
    print(f"  {tool} reboot bootloader")
    print("Unlock (WARNING: erases all user data):")
    print(f"  {tool} oem unlock")
    print("Flash staged boot partitions:")
    print(f"  {tool} --wait flash sbl {fw}\\sbl.bin")
    print(f"  {tool} --wait flash rec {fw}\\rec.bin")
    print("Flash direct partitions:")
    print(f"  {tool} flash system {fw}\\system.bin")
    print("Erase direct partitions:")
    print(f"  {tool} erase rec")
    print(f"  {tool} erase system")
    print(f"  {tool} erase tmp")
    print("Raw command example:")
    print(f"  {tool} getvar all")


def main():
    try:
        return _main()
    except KeyboardInterrupt:
        print("\nsbltool: interrupted", file=sys.stderr)
        return 130


def _main():
    parser = argparse.ArgumentParser(prog="sbltool", description="Talk to TOS SBL FASTBOOT CDC.")
    parser.add_argument("--version", action="version", version=f"sbltool {__version__}")
    parser.add_argument("-s", "--serial", help="target device serial number")
    parser.add_argument("-p", "--port", help=argparse.SUPPRESS)
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
        help="devices, getvar, flash, erase, reboot and oem commands",
    )
    args = parser.parse_args()

    words = args.words or ["getvar", "all"]
    command = " ".join(words).strip().lower()

    if command == "examples":
        print_examples()
        return 0

    serial, list_ports = import_serial()

    if command in ("devices", "ports"):
        try:
            for port in list_ports.comports():
                try:
                    ser = serial.Serial(port.device, args.baud, timeout=0.05, write_timeout=0.5)
                except (serial.SerialException, OSError, PermissionError):
                    continue
                try:
                    serial_number = getvar(ser, "serialno")
                    product = getvar(ser, "product")
                    if re.fullmatch(r"[0-9A-F]{24}", serial_number) and product:
                        print(f"{serial_number}\tfastboot")
                except Exception:
                    continue
                finally:
                    ser.close()
        except KeyboardInterrupt:
            print("\nsbltool: interrupted", file=sys.stderr)
            return 130
        return 0

    port = args.port
    if not port:
        for candidate in list_ports.comports():
            probe = None
            try:
                probe = serial.Serial(candidate.device, args.baud, timeout=0.05,
                                      write_timeout=0.5)
                serial_number = getvar(probe, "serialno")
                if not args.serial or serial_number.upper() == args.serial.upper():
                    port = candidate.device
                    break
            except Exception:
                continue
            finally:
                if probe is not None:
                    probe.close()
    if not port:
        print("sbltool: no matching fastboot device", file=sys.stderr)
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
                    print("Usage: sbltool flash <partition> <image>", file=sys.stderr)
                    return 2
                rc = do_flash(ser, words[1], words[2])
                if rc == 0 and args.wait:
                    return do_wait_fastboot(serial, list_ports, port, args.baud, args.wait_timeout)
                return rc
            if words[0].lower() == "erase":
                if len(words) != 2:
                    print("Usage: sbltool erase <partition>", file=sys.stderr)
                    return 2
                part = words[1].lower()
                erase_timeout = 90.0 if part in ("system", "tmp") else 25.0
                rsp = send_ascii_command(ser, f"ERASE {part}\n", timeout=erase_timeout, expect_final=True)
                return 0 if rsp.startswith("OKAY") else 1

            if command == "getvar all":
                names = ("product", "version", "version-bootloader",
                         "version-baseband", "serialno", "unlocked")
                for name in names:
                    print(f"(bootloader) {name}: {getvar(ser, name)}")
                return 0
            if words[0].lower() == "getvar" and len(words) == 1:
                print("Usage: sbltool getvar <variable|all>", file=sys.stderr)
                return 2
            if command == "partitions":
                for item in query_partitions(ser):
                    print(":".join(f"{key}:{value}" for key, value in item.items()))
                return 0
            if words[0].lower() == "getvar" and len(words) == 2:
                print(getvar(ser, words[1]))
                return 0
            if command == "reboot":
                rsp = send_ascii_command(ser, "REBOOT\n", timeout=2.0, expect_final=False)
                print(rsp, end="")
                return 0
            if command in ("reboot recovery", "reboot-recovery"):
                rsp = send_ascii_command(ser, "REBOOT RECOVERY\n", timeout=2.0, expect_final=False)
                print(rsp, end="")
                return 0
            if command in ("reboot bootloader", "reboot-bootloader"):
                rsp = send_ascii_command(ser, "REBOOT BOOTLOADER\n", timeout=2.0, expect_final=False)
                print(rsp, end="")
                return 0
            if command in ("oem unlock", "oem-unlock"):
                if getvar(ser, "unlocked") == "yes":
                    print("Bootloader is already unlocked.")
                    return 0
                print("")
                print("======== UNLOCK BOOTLOADER ========")
                print("This operation will delete all")
                print("personal data on your device to prevent")
                print("unauthorized access, then you can")
                print("install new operating system software")
                print("on the device.")
                print("====================================")
                print("")
                try:
                    answer = input("Are you sure you want to unlock? (yes/no): ").strip().lower()
                except (EOFError, KeyboardInterrupt):
                    print("\nsbltool: unlock aborted", file=sys.stderr)
                    return 130
                if answer not in ("yes", "y"):
                    print("Unlock canceled.")
                    return 0
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
        except KeyboardInterrupt:
            print("\nsbltool: interrupted", file=sys.stderr)
            return 130
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
