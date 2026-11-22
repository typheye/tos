"""Shared metadata and resource paths for the TOS desktop tools."""
from pathlib import Path
import json
import sys


def resource(package: str, *parts: str) -> Path:
    base = Path(sys._MEIPASS) if getattr(sys, "frozen", False) else Path(__file__).resolve().parent.parent
    return base.joinpath(package, *parts)


def metadata(package: str) -> dict:
    return json.loads(resource(package, "metadata.json").read_text(encoding="utf-8"))
