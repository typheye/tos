"""DFU flashing backends used by the Qt GUI."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, List, Optional, Sequence

from .elf2bin import ConversionInfo, convert_elf_to_bin


LogFn = Callable[[str], None]


class FlashError(RuntimeError):
    """Raised when detect/flash operation fails."""


@dataclass
class ToolConfig:
    flash_base: int = 0x08000000
    flash_size: int = 1024 * 1024
    dfu_vid_pid: str = "0483:df11"
    dfu_alt: str = "0"
    dfu_util_path: str = ""
    stm32_cli_path: str = ""


@dataclass
class FlashResult:
    ok: bool
    backend: str
    elapsed_seconds: float
    message: str
    converted_bin: Optional[Path] = None


def tools_root() -> Path:
    # Package path: DFU_tools/tos_dfu_gui/dfu.py
    return Path(__file__).resolve().parents[1]


def project_root() -> Path:
    return tools_root().parent


def load_config() -> ToolConfig:
    cfg = ToolConfig()
    path = tools_root() / "config.json"
    if not path.exists():
        return cfg
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return cfg

    def parse_int(value, default):
        if isinstance(value, int):
            return value
        if isinstance(value, str):
            return int(value, 0)
        return default

    cfg.flash_base = parse_int(raw.get("flash_base"), cfg.flash_base)
    cfg.flash_size = parse_int(raw.get("flash_size"), cfg.flash_size)
    cfg.dfu_vid_pid = str(raw.get("dfu_vid_pid", cfg.dfu_vid_pid))
    cfg.dfu_alt = str(raw.get("dfu_alt", cfg.dfu_alt))
    cfg.dfu_util_path = str(raw.get("dfu_util_path", cfg.dfu_util_path))
    cfg.stm32_cli_path = str(raw.get("stm32_cli_path", cfg.stm32_cli_path))
    return cfg


def parse_hex_int(text: str) -> int:
    text = text.strip()
    if not text:
        raise ValueError("空数字")
    return int(text, 0)


def _candidate_executable_names(name: str) -> List[str]:
    if os.name == "nt" and not name.lower().endswith(".exe"):
        return [name + ".exe", name]
    return [name]


def _local_bin_candidates(name: str) -> Iterable[Path]:
    bin_dir = tools_root() / "bin"
    for candidate in _candidate_executable_names(name):
        yield bin_dir / candidate


def find_executable(config_path: str, executable_name: str, common_paths: Sequence[Path] = ()) -> Optional[Path]:
    if config_path:
        configured = Path(config_path).expanduser()
        if configured.exists():
            return configured

    for candidate in _local_bin_candidates(executable_name):
        if candidate.exists():
            return candidate

    found = shutil.which(executable_name)
    if found:
        return Path(found)
    if os.name == "nt" and not executable_name.lower().endswith(".exe"):
        found = shutil.which(executable_name + ".exe")
        if found:
            return Path(found)

    for candidate in common_paths:
        if candidate.exists():
            return candidate
    return None


def find_dfu_util(config: ToolConfig) -> Optional[Path]:
    return find_executable(config.dfu_util_path, "dfu-util")


def find_stm32_cli(config: ToolConfig) -> Optional[Path]:
    common = []
    if os.name == "nt":
        common.extend(
            [
                Path(r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"),
                Path(r"C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"),
            ]
        )
    return find_executable(config.stm32_cli_path, "STM32_Programmer_CLI", common)


def default_firmware_candidates() -> List[Path]:
    root = project_root()
    candidates = [
        root / "build" / "tos.elf",
        root / "build" / "Debug" / "tos.elf",
        root / "build" / "Release" / "tos.elf",
        root / "cmake-build-debug" / "tos.elf",
        root / "cmake-build-release" / "tos.elf",
        root / "Debug" / "tos.elf",
        root / "Release" / "tos.elf",
    ]
    # Keep exact candidates first, then recursive fallback.
    seen = set()
    result: List[Path] = []
    for p in candidates:
        if p not in seen:
            seen.add(p)
            result.append(p)
    for p in root.glob("**/tos.elf"):
        if "DFU_tools" in p.parts:
            continue
        if p not in seen:
            seen.add(p)
            result.append(p)
    return result


def find_default_firmware() -> Optional[Path]:
    for path in default_firmware_candidates():
        if path.exists():
            return path
    return None


def _run_command(args: Sequence[str], log: LogFn, *, cwd: Optional[Path] = None, timeout: Optional[int] = None) -> str:
    log("$ " + " ".join(f'\"{a}\"' if " " in a else a for a in args))
    try:
        proc = subprocess.Popen(
            list(args),
            cwd=str(cwd) if cwd else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except FileNotFoundError as exc:
        raise FlashError(f"找不到工具：{args[0]}") from exc

    start = time.monotonic()
    chunks: List[str] = []
    assert proc.stdout is not None
    while True:
        line = proc.stdout.readline()
        if line:
            chunks.append(line)
            log(line.rstrip("\n"))
        elif proc.poll() is not None:
            break
        if timeout is not None and time.monotonic() - start > timeout:
            proc.kill()
            raise FlashError("命令执行超时，已终止。")

    # Drain any remaining output.
    rest = proc.stdout.read()
    if rest:
        chunks.append(rest)
        for line in rest.splitlines():
            log(line)

    if proc.returncode != 0:
        output = "".join(chunks).strip()
        raise FlashError(f"命令失败，退出码 {proc.returncode}。\n{output}")
    return "".join(chunks)


def detect_with_dfu_util(config: ToolConfig, log: LogFn) -> str:
    dfu_util = find_dfu_util(config)
    if not dfu_util:
        raise FlashError("找不到 dfu-util。请把 dfu-util.exe 放到 DFU_tools/bin，或加入 PATH。")
    output = _run_command([str(dfu_util), "-l", "-d", config.dfu_vid_pid], log, timeout=20)
    if config.dfu_vid_pid.lower() not in output.lower() and "Found DFU" not in output:
        raise FlashError("没有检测到 STM32 DFU 设备。请确认 BOOT0/DFU 模式和 USB 连接。")
    return output


def detect_with_stm32_cli(config: ToolConfig, log: LogFn) -> str:
    cli = find_stm32_cli(config)
    if not cli:
        raise FlashError("找不到 STM32_Programmer_CLI。请安装 STM32CubeProgrammer 或配置工具路径。")
    return _run_command([str(cli), "-l"], log, timeout=20)


def flash_with_dfu_util(elf_path: Path, config: ToolConfig, log: LogFn) -> FlashResult:
    started = time.monotonic()
    dfu_util = find_dfu_util(config)
    if not dfu_util:
        raise FlashError("找不到 dfu-util。请把 dfu-util.exe 放到 DFU_tools/bin，或加入 PATH。")

    elf_path = Path(elf_path)
    with tempfile.TemporaryDirectory(prefix="tos_dfu_") as temp_name:
        bin_path = Path(temp_name) / (elf_path.stem + ".bin")
        log("正在将 ELF 转换为 BIN...")
        info: ConversionInfo = convert_elf_to_bin(
            elf_path,
            bin_path,
            flash_base=config.flash_base,
            flash_size=config.flash_size,
        )
        log(
            "转换完成："
            f"{info.segment_count} 个 Flash 段，"
            f"0x{info.lowest_address:08X}-0x{info.highest_address:08X}，"
            f"BIN {info.binary_size} 字节。"
        )

        # Keep a copy beside the ELF so the user can inspect/reuse it.
        persistent_bin = elf_path.with_suffix(".dfu.bin")
        persistent_bin.write_bytes(bin_path.read_bytes())
        log(f"已生成：{persistent_bin}")

        log("正在检测 DFU 设备...")
        detect_with_dfu_util(config, log)

        address_spec = f"0x{config.flash_base:08X}:leave"
        args = [
            str(dfu_util),
            "-d",
            config.dfu_vid_pid,
            "-a",
            config.dfu_alt,
            "-s",
            address_spec,
            "-D",
            str(persistent_bin),
        ]
        log("开始通过 DFU 写入内部 Flash...")
        _run_command(args, log, timeout=180)
        elapsed = time.monotonic() - started
        return FlashResult(True, "dfu-util", elapsed, "刷写完成，设备应已退出 DFU 并重启。", persistent_bin)


def flash_with_stm32_cli(elf_path: Path, config: ToolConfig, log: LogFn) -> FlashResult:
    started = time.monotonic()
    cli = find_stm32_cli(config)
    if not cli:
        raise FlashError("找不到 STM32_Programmer_CLI。请安装 STM32CubeProgrammer 或配置工具路径。")
    elf_path = Path(elf_path)
    if not elf_path.exists():
        raise FlashError(f"找不到 ELF 文件：{elf_path}")

    args = [
        str(cli),
        "-c",
        "port=USB1",
        "-w",
        str(elf_path),
        "-v",
        "-rst",
    ]
    log("开始通过 STM32CubeProgrammer CLI 写入...")
    _run_command(args, log, timeout=180)
    elapsed = time.monotonic() - started
    return FlashResult(True, "STM32_Programmer_CLI", elapsed, "刷写完成，已请求复位。")
