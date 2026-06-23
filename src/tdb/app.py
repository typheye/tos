#!/usr/bin/env python3
"""TOS Debug Bridge host client for the REC CDC interface."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import sys
import time
import zlib
from dataclasses import dataclass
from typing import BinaryIO, Optional

VID = 0x0483
PID = 0x5756
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 5.0
PUSH_CHUNK = 1024
CONFIG_PATH = pathlib.Path.home() / ".tdb_device.json"


class TDBError(RuntimeError):
    pass


def import_serial():
    try:
        import serial
        from serial.tools import list_ports
    except ImportError as exc:
        raise TDBError("pyserial is required: python -m pip install pyserial") from exc
    return serial, list_ports


def crc32(data: bytes, value: int = 0) -> int:
    return zlib.crc32(data, value) & 0xFFFFFFFF


def encode_text(text: str) -> str:
    return text.encode("utf-8").hex().upper()


def load_saved_port() -> Optional[str]:
    try:
        data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError, TypeError):
        return None
    value = data.get("port")
    return value if isinstance(value, str) and value else None


def save_port(port: str, baud: int) -> None:
    data = {"port": port, "baud": baud}
    try:
        CONFIG_PATH.write_text(json.dumps(data, indent=2), encoding="utf-8")
    except OSError as exc:
        raise TDBError(f"Connected, but cannot save {CONFIG_PATH}: {exc}") from exc


def clear_saved_port() -> None:
    try:
        CONFIG_PATH.unlink()
    except FileNotFoundError:
        pass
    except OSError as exc:
        raise TDBError(f"Cannot remove {CONFIG_PATH}: {exc}") from exc


def find_port(list_ports) -> Optional[str]:
    candidates = []
    for port in list_ports.comports():
        text = " ".join(
            str(value or "")
            for value in (
                port.description,
                port.manufacturer,
                port.product,
                port.interface,
                port.hwid,
            )
        ).lower()
        if port.vid == VID and port.pid == PID:
            return port.device
        if "tos debug bridge" in text or "tos-tdb" in text or "tdb cdc" in text:
            candidates.append(port.device)
    return candidates[0] if candidates else None


def resolve_port(requested: Optional[str], list_ports) -> str:
    if requested:
        return requested
    saved = load_saved_port()
    if saved:
        return saved
    detected = find_port(list_ports)
    if detected:
        return detected
    raise TDBError("No TOS Debug Bridge COM port found. Use --port COMx.")


@dataclass
class Response:
    ok: bool
    message: str
    data: bytes = b""


class TDBClient:
    def __init__(self, port: str, baud: int = DEFAULT_BAUD, timeout: float = DEFAULT_TIMEOUT):
        serial, _ = import_serial()
        self._serial_module = serial
        self.port = port
        self.baud = baud
        self.timeout = timeout
        try:
            self.ser = serial.Serial(
                port,
                baud,
                timeout=0.05,
                write_timeout=max(timeout, 3.0),
            )
        except (serial.SerialException, OSError, PermissionError) as exc:
            raise TDBError(f"Cannot open {port}: {exc}") from exc
        self._line_buffer = bytearray()

    def close(self) -> None:
        try:
            self.ser.close()
        except Exception:
            pass

    def __enter__(self) -> "TDBClient":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def _write(self, data: bytes) -> None:
        try:
            self.ser.write(data)
            self.ser.flush()
        except (self._serial_module.SerialException, OSError) as exc:
            raise TDBError(f"Serial write failed: {exc}") from exc

    def send_line(self, line: str) -> None:
        self._write(line.encode("ascii") + b"\n")

    def read_line(self, timeout: Optional[float] = None) -> str:
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        while time.monotonic() < deadline:
            try:
                chunk = self.ser.read(1)
            except (self._serial_module.SerialException, OSError) as exc:
                raise TDBError(f"Serial read failed: {exc}") from exc
            if not chunk:
                time.sleep(0.002)
                continue
            if chunk == b"\n":
                line = self._line_buffer.rstrip(b"\r").decode("ascii", errors="replace")
                self._line_buffer.clear()
                return line
            self._line_buffer.extend(chunk)
            if len(self._line_buffer) > 4096:
                self._line_buffer.clear()
                raise TDBError("Protocol line is too long")
        raise TDBError("Timed out waiting for TDB response")

    def read_exact(self, size: int, timeout: Optional[float] = None) -> bytes:
        deadline = time.monotonic() + (
            max(self.timeout, 10.0 + size / 20_000.0) if timeout is None else timeout
        )
        result = bytearray()
        while len(result) < size and time.monotonic() < deadline:
            try:
                chunk = self.ser.read(min(4096, size - len(result)))
            except (self._serial_module.SerialException, OSError) as exc:
                raise TDBError(f"Serial read failed: {exc}") from exc
            if chunk:
                result.extend(chunk)
                continue
            time.sleep(0.002)
        if len(result) != size:
            raise TDBError(f"Timed out reading payload: expected {size}, got {len(result)}")
        return bytes(result)

    @staticmethod
    def _parse_status(line: str) -> Response:
        if line == "OKAY" or line.startswith("OKAY "):
            return Response(True, line[5:] if len(line) > 4 else "")
        if line == "FAIL" or line.startswith("FAIL "):
            return Response(False, line[5:] if len(line) > 4 else "")
        raise TDBError(f"Unexpected TDB response: {line!r}")

    def _next_protocol_line(self, timeout: Optional[float] = None) -> str:
        while True:
            line = self.read_line(timeout)
            if not line:
                continue
            if line.startswith("TOS-TDB READY"):
                continue
            return line

    def response(self, timeout: Optional[float] = None) -> Response:
        line = self._next_protocol_line(timeout)
        if line.startswith("DATA "):
            parts = line.split()
            if len(parts) != 3:
                raise TDBError(f"Malformed DATA header: {line!r}")
            try:
                size = int(parts[1], 10)
                expected_crc = int(parts[2], 0)
            except ValueError as exc:
                raise TDBError(f"Malformed DATA header: {line!r}") from exc
            data = self.read_exact(size)
            actual_crc = crc32(data)
            status = self._parse_status(self._next_protocol_line(timeout))
            if actual_crc != expected_crc:
                raise TDBError(
                    f"Payload CRC mismatch: expected 0x{expected_crc:08X}, "
                    f"got 0x{actual_crc:08X}"
                )
            status.data = data
            return status
        return self._parse_status(line)

    def command(self, line: str, timeout: Optional[float] = None) -> Response:
        self.send_line(line)
        return self.response(timeout)

    def handshake(self) -> Response:
        # Let Windows finish the CDC open sequence and discard any stale banner.
        time.sleep(0.08)
        try:
            self.ser.reset_input_buffer()
        except Exception:
            pass
        response = self.command("HELLO", timeout=max(self.timeout, 3.0))
        if not response.ok or not response.message.startswith("TDB/1"):
            raise TDBError(f"Unexpected device handshake: {response.message}")
        return response

    def shell(self, command: str) -> Response:
        return self.command(f"SHELL {encode_text(command)}", timeout=max(self.timeout, 30.0))

    def push(self, local: pathlib.Path, remote: str, progress: bool = True) -> None:
        try:
            size = local.stat().st_size
        except OSError as exc:
            raise TDBError(f"Cannot stat {local}: {exc}") from exc
        checksum = 0
        try:
            with local.open("rb") as source:
                while True:
                    chunk = source.read(1024 * 1024)
                    if not chunk:
                        break
                    checksum = crc32(chunk, checksum)
        except OSError as exc:
            raise TDBError(f"Cannot read {local}: {exc}") from exc

        response = self.command(
            f"PUSHBEGIN {size} 0x{checksum:08X} {encode_text(remote)}",
            timeout=max(self.timeout, 15.0),
        )
        if not response.ok:
            raise TDBError(f"Push rejected: {response.message}")

        sent = 0
        try:
            with local.open("rb") as source:
                while True:
                    chunk = source.read(PUSH_CHUNK)
                    if not chunk:
                        break
                    chunk_crc = crc32(chunk)
                    response = self.command(
                        f"PUSHDATA {len(chunk)} {sent} 0x{chunk_crc:08X}",
                        timeout=max(self.timeout, 10.0),
                    )
                    if not response.ok or not response.message.startswith("SEND"):
                        raise TDBError(f"Chunk rejected at {sent}: {response.message}")
                    self._write(chunk)
                    response = self.response(timeout=max(self.timeout, 20.0))
                    if not response.ok:
                        raise TDBError(f"Chunk write failed at {sent}: {response.message}")
                    sent += len(chunk)
                    if progress:
                        percent = 100 if size == 0 else sent * 100 // size
                        print(f"\rpush {sent}/{size} bytes ({percent:3d}%)", end="", flush=True)
        except OSError as exc:
            try:
                self.command("ABORT")
            except TDBError:
                pass
            raise TDBError(f"Cannot read {local}: {exc}") from exc

        response = self.command("PUSHEND", timeout=max(self.timeout, 20.0))
        if progress:
            print()
        if not response.ok:
            raise TDBError(f"Push finalization failed: {response.message}")

    def pull(self, remote: str, destination: BinaryIO, progress: bool = True) -> int:
        self.send_line(f"PULL {encode_text(remote)}")
        line = self._next_protocol_line(timeout=max(self.timeout, 15.0))
        if not line.startswith("DATA "):
            response = self._parse_status(line)
            raise TDBError(response.message or "Pull failed")
        parts = line.split()
        if len(parts) != 3:
            raise TDBError(f"Malformed DATA header: {line!r}")
        try:
            size = int(parts[1], 10)
            expected_crc = int(parts[2], 0)
        except ValueError as exc:
            raise TDBError(f"Malformed DATA header: {line!r}") from exc

        received = 0
        checksum = 0
        deadline = time.monotonic() + max(30.0, size / 10_000.0 + 20.0)
        while received < size:
            if time.monotonic() >= deadline:
                raise TDBError(f"Timed out pulling file at {received}/{size} bytes")
            try:
                chunk = self.ser.read(min(4096, size - received))
            except (self._serial_module.SerialException, OSError) as exc:
                raise TDBError(f"Serial read failed: {exc}") from exc
            if not chunk:
                time.sleep(0.002)
                continue
            destination.write(chunk)
            checksum = crc32(chunk, checksum)
            received += len(chunk)
            if progress:
                percent = 100 if size == 0 else received * 100 // size
                print(f"\rpull {received}/{size} bytes ({percent:3d}%)", end="", flush=True)

        response = self._parse_status(self._next_protocol_line(timeout=max(self.timeout, 10.0)))
        if progress:
            print()
        if not response.ok:
            raise TDBError(f"Pull failed: {response.message}")
        if checksum != expected_crc:
            raise TDBError(
                f"Pulled file CRC mismatch: expected 0x{expected_crc:08X}, "
                f"got 0x{checksum:08X}"
            )
        return size


def add_connection_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("-p", "--port", help="COM port, for example COM8")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)


def open_client(args) -> TDBClient:
    _, list_ports = import_serial()
    port = resolve_port(getattr(args, "port", None), list_ports)
    client = TDBClient(port, getattr(args, "baud", DEFAULT_BAUD), getattr(args, "timeout", DEFAULT_TIMEOUT))
    try:
        client.handshake()
    except Exception:
        client.close()
        raise
    return client


def command_devices(_args) -> int:
    _, list_ports = import_serial()
    found = False
    for port in list_ports.comports():
        text = " ".join(str(v or "") for v in (port.description, port.product, port.hwid))
        marker = "TDB" if port.vid == VID and port.pid == PID else ""
        print(
            f"{port.device:10} vid={str(port.vid):>6} pid={str(port.pid):>6} "
            f"{marker:3} {text}"
        )
        found = True
    if not found:
        print("No serial ports found.")
    return 0


def command_connect(args) -> int:
    with open_client(args) as client:
        save_port(client.port, client.baud)
        info = client.command("INFO")
        print(f"connected to {client.port} ({info.message or 'TDB/1'})")
        if info.data:
            print(info.data.decode("utf-8", errors="replace"), end="")
    return 0


def command_disconnect(_args) -> int:
    clear_saved_port()
    print("disconnected")
    return 0


def print_shell_data(data: bytes) -> None:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
        return
    print(text, end="" if text.endswith("\n") or not text else "\n")


def command_shell(args) -> int:
    with open_client(args) as client:
        if args.command:
            command = " ".join(args.command)
            response = client.shell(command)
            print_shell_data(response.data)
            if not response.ok:
                print(f"tdb: {response.message or 'command failed'}", file=sys.stderr)
                return 1
            return 0

        cwd = "0:/"
        try:
            response = client.shell("pwd")
            if response.ok:
                cwd = response.data.decode("utf-8", errors="replace").strip() or cwd
        except TDBError:
            pass
        print(f"TOS Debug Bridge shell on {client.port}. Type 'help' or 'exit'.")
        while True:
            try:
                line = input(f"tdb:{cwd}> ").strip()
            except EOFError:
                print()
                break
            except KeyboardInterrupt:
                print()
                continue
            if not line:
                continue
            if line in ("exit", "quit"):
                break
            try:
                response = client.shell(line)
            except TDBError as exc:
                print(f"tdb: {exc}", file=sys.stderr)
                return 1
            print_shell_data(response.data)
            if not response.ok:
                print(f"tdb: {response.message or 'command failed'}", file=sys.stderr)
            if response.ok and line.split(maxsplit=1)[0].lower() in ("cd", "mount", "umount"):
                try:
                    pwd = client.shell("pwd")
                    if pwd.ok:
                        cwd = pwd.data.decode("utf-8", errors="replace").strip() or cwd
                except TDBError:
                    pass
    return 0


def command_push(args) -> int:
    local = pathlib.Path(args.local)
    remote = args.remote or f"0:/{local.name}"
    with open_client(args) as client:
        client.push(local, remote, progress=not args.quiet)
        print(f"{local} -> {remote}")
    return 0


def command_pull(args) -> int:
    remote = args.remote
    local = pathlib.Path(args.local or pathlib.PurePosixPath(remote).name or "tdb-pull.bin")
    temp = local.with_name(local.name + ".tdbtmp")
    try:
        with open_client(args) as client, temp.open("wb") as destination:
            size = client.pull(remote, destination, progress=not args.quiet)
        os.replace(temp, local)
    except Exception:
        try:
            temp.unlink()
        except OSError:
            pass
        raise
    print(f"{remote} -> {local} ({size} bytes)")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="tdb", description="TOS Debug Bridge for REC")
    parser.add_argument("--version", action="version", version="tdb 1.0")
    sub = parser.add_subparsers(dest="subcommand", required=True)

    p = sub.add_parser("devices", help="list serial ports")
    p.set_defaults(func=command_devices)

    p = sub.add_parser("connect", help="connect and remember a TDB COM port")
    add_connection_options(p)
    p.set_defaults(func=command_connect)

    p = sub.add_parser("disconnect", help="forget the remembered COM port")
    p.set_defaults(func=command_disconnect)

    p = sub.add_parser("shell", help="open an interactive shell or run one command")
    add_connection_options(p)
    p.add_argument("command", nargs=argparse.REMAINDER, help="optional command to run")
    p.set_defaults(func=command_shell)

    p = sub.add_parser("push", help="copy a local file to the SD filesystem")
    add_connection_options(p)
    p.add_argument("local")
    p.add_argument("remote", nargs="?")
    p.add_argument("-q", "--quiet", action="store_true")
    p.set_defaults(func=command_push)

    p = sub.add_parser("pull", help="copy a file from the SD filesystem")
    add_connection_options(p)
    p.add_argument("remote")
    p.add_argument("local", nargs="?")
    p.add_argument("-q", "--quiet", action="store_true")
    p.set_defaults(func=command_pull)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return int(args.func(args))
    except TDBError as exc:
        print(f"tdb: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\ntdb: interrupted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
