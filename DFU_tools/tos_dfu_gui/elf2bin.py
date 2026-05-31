"""Minimal ELF32-to-BIN converter for STM32 firmware images.

Why this exists:
    dfu-util writes raw bytes; it does not understand ELF load addresses.
    The converter extracts PT_LOAD segments whose load address belongs to
    internal Flash and creates a contiguous .bin starting at 0x08000000.

It intentionally supports the normal ARM GCC output used by STM32Cube/CMake:
    * 32-bit little-endian ELF
    * program headers with PT_LOAD segments
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Tuple


ELF_MAGIC = b"\x7fELF"
PT_LOAD = 1
ELFCLASS32 = 1
ELFDATA2LSB = 1


class ElfConversionError(RuntimeError):
    """Raised when an ELF file cannot be converted safely."""


@dataclass(frozen=True)
class LoadSegment:
    load_address: int
    data: bytes


@dataclass(frozen=True)
class ConversionInfo:
    output_path: Path
    flash_base: int
    lowest_address: int
    highest_address: int
    binary_size: int
    segment_count: int
    entry_point: int


def _read_elf32_load_segments(elf_path: Path) -> Tuple[int, List[LoadSegment]]:
    data = elf_path.read_bytes()
    if len(data) < 52:
        raise ElfConversionError("ELF 文件太小，无法识别。")
    if data[:4] != ELF_MAGIC:
        raise ElfConversionError("输入文件不是 ELF。请确认选择的是 tos.elf。")
    if data[4] != ELFCLASS32:
        raise ElfConversionError("暂不支持 ELF64；STM32F407 工程应输出 ELF32。")
    if data[5] != ELFDATA2LSB:
        raise ElfConversionError("暂不支持大端 ELF；STM32F407 工程应为 little-endian。")

    header = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    (
        _e_type,
        _e_machine,
        _e_version,
        e_entry,
        e_phoff,
        _e_shoff,
        _e_flags,
        _e_ehsize,
        e_phentsize,
        e_phnum,
        _e_shentsize,
        _e_shnum,
        _e_shstrndx,
    ) = header

    if e_phoff == 0 or e_phnum == 0:
        raise ElfConversionError("ELF 中没有 program header，无法定位 Flash 段。")
    if e_phentsize < 32:
        raise ElfConversionError("ELF program header 尺寸异常。")

    segments: List[LoadSegment] = []
    for idx in range(e_phnum):
        offset = e_phoff + idx * e_phentsize
        if offset + 32 > len(data):
            raise ElfConversionError("ELF program header 越界，文件可能损坏。")
        (
            p_type,
            p_offset,
            p_vaddr,
            p_paddr,
            p_filesz,
            _p_memsz,
            _p_flags,
            _p_align,
        ) = struct.unpack_from("<IIIIIIII", data, offset)
        if p_type != PT_LOAD or p_filesz == 0:
            continue
        if p_offset + p_filesz > len(data):
            raise ElfConversionError("ELF LOAD 段越界，文件可能损坏。")

        # For STM32 linker scripts, .data often has VMA in RAM but LMA in Flash.
        # p_paddr is usually the Flash load address; fall back to p_vaddr only
        # when p_paddr is zero.
        load_address = p_paddr or p_vaddr
        segments.append(LoadSegment(load_address, data[p_offset : p_offset + p_filesz]))

    if not segments:
        raise ElfConversionError("ELF 中没有可刷写的 LOAD 段。")
    return e_entry, segments


def convert_elf_to_bin(
    elf_path: Path,
    output_path: Path,
    *,
    flash_base: int = 0x08000000,
    flash_size: int = 1024 * 1024,
    fill_byte: int = 0xFF,
) -> ConversionInfo:
    """Convert an STM32 ELF file to a raw binary image.

    Only LOAD segments whose load address falls inside
    [flash_base, flash_base + flash_size) are included.
    """
    elf_path = Path(elf_path)
    output_path = Path(output_path)
    if not elf_path.exists():
        raise ElfConversionError(f"找不到 ELF 文件：{elf_path}")

    entry_point, segments = _read_elf32_load_segments(elf_path)
    flash_end = flash_base + flash_size
    flash_segments = [
        seg
        for seg in segments
        if flash_base <= seg.load_address < flash_end and seg.load_address + len(seg.data) <= flash_end
    ]

    if not flash_segments:
        detail = ", ".join(f"0x{s.load_address:08X}" for s in segments[:6])
        raise ElfConversionError(
            "没有找到位于内部 Flash 的 LOAD 段。"
            f" 当前 Flash 基址为 0x{flash_base:08X}，已看到段地址：{detail}"
        )

    lowest = min(seg.load_address for seg in flash_segments)
    highest = max(seg.load_address + len(seg.data) for seg in flash_segments)

    if lowest < flash_base:
        raise ElfConversionError("ELF 段地址低于 Flash 基址。")
    total_size = highest - flash_base
    if total_size <= 0 or total_size > flash_size:
        raise ElfConversionError("生成 BIN 尺寸异常，已拒绝转换。")

    image = bytearray([fill_byte]) * total_size
    for seg in flash_segments:
        start = seg.load_address - flash_base
        image[start : start + len(seg.data)] = seg.data

    # A light sanity check for STM32 vector table.
    if len(image) >= 8:
        initial_sp, reset_handler = struct.unpack_from("<II", image, 0)
        if not (0x20000000 <= initial_sp <= 0x20030000):
            raise ElfConversionError(
                "ELF 转 BIN 后的向量表看起来不对："
                f" initial_sp=0x{initial_sp:08X}。请确认 Flash 基址是否为 0x08000000。"
            )
        if not (flash_base <= reset_handler < flash_end):
            raise ElfConversionError(
                "ELF 转 BIN 后的 Reset_Handler 地址看起来不在 Flash："
                f" reset=0x{reset_handler:08X}。"
            )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(bytes(image))
    return ConversionInfo(
        output_path=output_path,
        flash_base=flash_base,
        lowest_address=lowest,
        highest_address=highest,
        binary_size=len(image),
        segment_count=len(flash_segments),
        entry_point=entry_point,
    )
