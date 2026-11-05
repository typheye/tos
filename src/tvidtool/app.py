#!/usr/bin/env python3
"""
TVID Tool - convert desktop video into a TOS-friendly .tvid stream.

The board-side decoder intentionally does almost no work: it reads a small
header, then streams RGB565 frames from SD card into the LCD tile buffer.

Requirements:
    OpenCV Python package, bundled when packaged with PyInstaller.

Examples:
    python app.py input.mp4 -o test.tvid
    python app.py input.mp4 -o test.tvid --fps 60 --size 240x240
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

try:
    import cv2  # type: ignore
except Exception as exc:  # pragma: no cover
    cv2 = None
    CV2_IMPORT_ERROR = exc
else:
    CV2_IMPORT_ERROR = None

__version__ = "0.1.0"

TVID_MAGIC = b"TVID"
TVID_VERSION = 1
TVID_HEADER_SIZE = 40
TVID_CODEC_RGB565_RAW = 1
TVID_CODEC_RGB565_RLE = 2
TVID_FLAGS_LOOP = 0x0001
TVID_MAX_FILE_SIZE = 0xFFFFFFFF

FRAME_MAGIC = b"FRM1"
FRAME_HEADER = struct.Struct("<4sIBBHII")
FILE_HEADER = struct.Struct("<4sHHHHHHIIIIII")


def parse_size(text: str) -> tuple[int, int]:
    try:
        w_s, h_s = text.lower().split("x", 1)
        w = int(w_s)
        h = int(h_s)
    except Exception as exc:
        raise argparse.ArgumentTypeError("size must look like 240x240") from exc
    if w <= 0 or h <= 0 or w > 1024 or h > 1024:
        raise argparse.ArgumentTypeError("size is out of range")
    return w, h


def require_cv2() -> None:
    if cv2 is None:
        raise RuntimeError(
            "OpenCV is not installed. Run: python -m pip install opencv-python"
        ) from CV2_IMPORT_ERROR


def bgr888_to_rgb565_be(raw: bytes) -> bytes:
    out = bytearray((len(raw) // 3) * 2)
    j = 0
    for i in range(0, len(raw), 3):
        b = raw[i]
        g = raw[i + 1]
        r = raw[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out[j] = (v >> 8) & 0xFF
        out[j + 1] = v & 0xFF
        j += 2
    return bytes(out)


def rle_rgb565(frame: bytes) -> bytes:
    if len(frame) % 2:
        raise ValueError("RGB565 frame length must be even")
    out = bytearray()
    pos = 0
    pixels = len(frame) // 2
    while pos < pixels:
        color0 = frame[pos * 2]
        color1 = frame[pos * 2 + 1]
        run = 1
        while pos + run < pixels and run < 65535:
            off = (pos + run) * 2
            if frame[off] != color0 or frame[off + 1] != color1:
                break
            run += 1
        out += struct.pack("<HBB", run, color0, color1)
        pos += run
    return bytes(out)


def fit_frame_bgr(frame, width: int, height: int):
    src_h, src_w = frame.shape[:2]
    scale = max(width / float(src_w), height / float(src_h))
    resized_w = max(width, int(round(src_w * scale)))
    resized_h = max(height, int(round(src_h * scale)))
    resized = cv2.resize(frame, (resized_w, resized_h), interpolation=cv2.INTER_AREA)
    x0 = max(0, (resized_w - width) // 2)
    y0 = max(0, (resized_h - height) // 2)
    return resized[y0 : y0 + height, x0 : x0 + width]


def iter_video_frames(
    src: Path,
    width: int,
    height: int,
    fps: int,
    start_frame: int = 0,
    end_frame: int | None = None,
):
    require_cv2()
    cap = cv2.VideoCapture(str(src))
    if not cap.isOpened():
        raise RuntimeError(f"cannot open video: {src}")
    src_fps = float(cap.get(cv2.CAP_PROP_FPS) or 0.0)
    if src_fps <= 0.1:
        src_fps = float(fps)
    source_per_output = src_fps / float(fps)
    frame_index = 0
    output_index = 0
    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                break
            fitted = None
            while int(output_index * source_per_output) == frame_index:
                if end_frame is not None and output_index >= end_frame:
                    return
                if output_index >= start_frame:
                    if fitted is None:
                        fitted = fit_frame_bgr(frame, width, height)
                    yield fitted
                output_index += 1
            frame_index += 1
    finally:
        cap.release()


def count_frames(
    src: Path,
    width: int,
    height: int,
    fps: int,
    start_frame: int = 0,
    end_frame: int | None = None,
) -> int:
    return sum(
        1
        for _ in iter_video_frames(
            src, width, height, fps, start_frame, end_frame
        )
    )


def convert(args: argparse.Namespace) -> int:
    src = Path(args.input).resolve()
    dst = Path(args.output).resolve()
    if not src.is_file():
        raise FileNotFoundError(src)

    width, height = args.size
    fps = int(args.fps)
    if fps <= 0 or fps > 60:
        raise ValueError("fps must be 1..60")
    start_frame = int(args.start_frame)
    end_frame = args.end_frame
    if start_frame < 0:
        raise ValueError("start-frame must be >= 0")
    if end_frame is not None:
        end_frame = int(end_frame)
        if end_frame <= start_frame:
            raise ValueError("end-frame must be greater than start-frame")

    codec = TVID_CODEC_RGB565_RLE if args.rle else TVID_CODEC_RGB565_RAW
    frame_count = count_frames(src, width, height, fps, start_frame, end_frame)
    if frame_count <= 0:
        raise RuntimeError("no frames decoded")

    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_suffix(dst.suffix + ".tmp")

    flags = TVID_FLAGS_LOOP if args.loop else 0

    written_frames = 0
    with tmp.open("wb") as f:
        f.write(
            FILE_HEADER.pack(
                TVID_MAGIC,
                TVID_VERSION,
                TVID_HEADER_SIZE,
                width,
                height,
                fps,
                flags,
                frame_count,
                codec,
                0,
                0,
                0,
                0,
            )
        )
        for frame in iter_video_frames(
            src, width, height, fps, start_frame, end_frame
        ):
            payload = bgr888_to_rgb565_be(frame.tobytes())
            frame_codec = TVID_CODEC_RGB565_RAW
            if args.rle:
                encoded = rle_rgb565(payload)
                # Short RLE runs save SD space but cost heavily on the F407.
                # Keep RLE only when its decode work is justified by a large
                # I/O reduction; natural video should normally stay RAW.
                if len(encoded) * 4 <= len(payload):
                    payload = encoded
                    frame_codec = TVID_CODEC_RGB565_RLE
            ts_ms = int((written_frames * 1000) / fps)
            f.write(
                FRAME_HEADER.pack(
                    FRAME_MAGIC,
                    ts_ms,
                    frame_codec,
                    0,
                    0,
                    len(payload),
                    width * height,
                )
            )
            f.write(payload)
            if f.tell() > TVID_MAX_FILE_SIZE:
                raise RuntimeError(
                    "TVID exceeds the FAT32/F407 4 GiB file-size limit; "
                    "use --end-frame, a shorter source, or --rle"
                )
            written_frames += 1
            if written_frames % 30 == 0:
                print(f"encoded {written_frames}/{frame_count} frames", flush=True)

    if written_frames != frame_count:
        raise RuntimeError(f"frame count changed: counted={frame_count}, written={written_frames}")

    tmp.replace(dst)
    print(f"TVID written: {dst}")
    print(f"  size: {width}x{height} @{fps}fps")
    print(f"  frames: {written_frames}")
    print(f"  codec: {'RGB565_RLE(auto)' if args.rle else 'RGB565_RAW'}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Convert video to TOS .tvid")
    p.add_argument("input", nargs="?", help="input video file")
    p.add_argument("-o", "--output", default="test.tvid", help="output .tvid path")
    p.add_argument("--size", type=parse_size, default=(240, 240), help="output size, default 240x240")
    p.add_argument("--fps", type=int, default=60, help="output fps, default 60")
    p.add_argument("--rle", action="store_true", help="try per-frame RGB565 RLE when smaller")
    p.add_argument(
        "--start-frame",
        type=int,
        default=0,
        help="first converted frame to include (default: 0)",
    )
    p.add_argument(
        "--end-frame",
        type=int,
        default=None,
        help="exclusive converted frame limit (default: until video end)",
    )
    p.add_argument("--no-loop", dest="loop", action="store_false", help="clear loop flag")
    p.add_argument("--version", action="store_true", help="print version and exit")
    p.set_defaults(loop=True)
    return p


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.version:
        print(f"tvidtool {__version__}")
        return 0
    if not args.input:
        parser.error("input is required unless --version is used")
    return convert(args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"[ERROR] {exc}", file=sys.stderr)
        raise SystemExit(1)
