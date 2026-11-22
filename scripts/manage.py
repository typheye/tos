#!/usr/bin/env python3
"""Manage native TOS desktop development, builds and user-local installation."""
from __future__ import annotations

import argparse
import importlib.metadata
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ("tos_helper", "tvidtool", "sbltool", "tdb")
ENV_NAME = "tos"
INSTALL_MARKER = ".tos-pc-install.json"


def host() -> str:
    if sys.platform not in ("linux", "win32"):
        raise RuntimeError(f"Unsupported platform: {sys.platform}; use Linux or Windows.")
    return sys.platform


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("+ " + subprocess.list2cmdline([str(x) for x in command]), flush=True)
    return subprocess.run([str(x) for x in command], check=True, cwd=ROOT, **kwargs)


def find_conda() -> str | None:
    for candidate in (os.environ.get("CONDA_EXE"), shutil.which("conda"),
                      str(Path(sys.prefix) / "condabin" / "conda.bat"),
                      str(Path(sys.prefix).parent.parent / "condabin" / "conda.bat"),
                      str(Path(sys.prefix).parent.parent / "bin" / "conda")):
        if candidate and Path(candidate).is_file():
            return candidate
    return None


def conda_command(conda: str, args: list[str]) -> list[str]:
    # Resolve the batch shim to a native entry point so spaces and metacharacters
    # remain separate argv values instead of being reparsed by cmd.exe.
    if sys.platform == "win32" and Path(conda).suffix.lower() in (".bat", ".cmd"):
        base = Path(conda).resolve().parent.parent
        for executable in (base / "Scripts" / "conda.exe", base / "_conda.exe"):
            if executable.is_file():
                return [str(executable), *args]
        python = base / "python.exe"
        if python.is_file():
            return [str(python), "-m", "conda", *args]
        raise RuntimeError("Cannot locate Conda's native launcher; set CONDA_EXE or use --current-env.")
    return [conda, *args]


def route_environment(args: argparse.Namespace, original: list[str]) -> int | None:
    if args.current_env or args.action in ("setup", "clean"):
        return None
    if os.environ.get("CONDA_DEFAULT_ENV") == ENV_NAME:
        return None
    conda = find_conda()
    if not conda:
        raise RuntimeError("Conda not found. Use --current-env with an existing Python environment, "
                           "or install Conda and run setup.")
    result = run(conda_command(conda, ["run", "--no-capture-output", "-n", ENV_NAME,
                 "python", str(ROOT / "scripts" / "manage.py"), "--current-env", *original]))
    return result.returncode


def setup(current_env: bool) -> None:
    if current_env:
        run([sys.executable, "-m", "pip", "install", "-r", str(ROOT / "requirement.txt")])
        run([sys.executable, "-c", "import tkinter; print('Tkinter available')"])
        return
    conda = find_conda()
    if not conda:
        raise RuntimeError("Conda not found. Use setup --current-env for an existing venv.")
    info = run(conda_command(conda, ["env", "list", "--json"]), capture_output=True, text=True)
    exists = any(Path(p).name == ENV_NAME for p in json.loads(info.stdout)["envs"])
    action = "update" if exists else "create"
    run(conda_command(conda, ["env", action, "-n", ENV_NAME, "-f", str(ROOT / "environment.yml")]))


def metadata(tool: str) -> dict:
    return json.loads((ROOT / "src" / tool / "metadata.json").read_text(encoding="utf-8"))


def artifacts(directory: Path) -> dict[str, Path]:
    suffix = ".exe" if host() == "win32" else ""
    return {tool: directory / ("" if tool == "tos_helper" else "platform-tools") /
            (metadata(tool)["executable"] + suffix) for tool in TOOLS}


def validate_artifacts(directory: Path) -> None:
    for tool, path in artifacts(directory).items():
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Missing or invalid artifact: {path}")
        if host() == "linux" and not os.access(path, os.X_OK):
            raise RuntimeError(f"Artifact is not executable: {path}")
        result = run([str(path), "--version"], capture_output=True, text=True, timeout=60)
        expected = metadata(tool)["version"]
        if expected not in result.stdout:
            raise RuntimeError(f"{path}: expected version {expected!r}, got {result.stdout!r}")
        print(result.stdout.strip())


def safe_path(path: Path, boundary: Path) -> Path:
    """Reject cleanup/replacement outside a boundary and every symlink in the path."""
    boundary = boundary.absolute()
    path = path.absolute()
    try:
        relative = path.relative_to(boundary)
    except ValueError as exc:
        raise RuntimeError(f"Refusing path outside {boundary}: {path}") from exc
    if not relative.parts or ".." in relative.parts:
        raise RuntimeError(f"Refusing unsafe target: {path}")
    current = boundary
    for part in relative.parts:
        current /= part
        if current.is_symlink():
            raise RuntimeError(f"Refusing symbolic link: {current}")
    return path


def remove_tree(path: Path, boundary: Path) -> None:
    path = safe_path(path, boundary)
    if path.is_dir():
        # Do not follow or remove even nested links during destructive cleanup.
        for base, dirs, files in os.walk(path, followlinks=False):
            for name in dirs + files:
                if (Path(base) / name).is_symlink():
                    raise RuntimeError(f"Refusing symbolic link inside cleanup target: {Path(base) / name}")
        shutil.rmtree(path)
    elif path.exists():
        raise RuntimeError(f"Expected a directory, refusing to delete: {path}")


def replace_directory(staged: Path, destination: Path) -> None:
    """Publish a fully validated directory; restore previous output on rename failure."""
    safe_path(destination, destination.parent)
    backup = destination.with_name(destination.name + ".previous")
    if backup.exists() or backup.is_symlink():
        raise RuntimeError(f"Previous backup exists; inspect before retrying: {backup}")
    old = destination.exists()
    if old:
        destination.rename(backup)
    try:
        staged.rename(destination)
    except BaseException:
        if old:
            backup.rename(destination)
        raise
    if old:
        remove_tree(backup, destination.parent)


def build() -> None:
    run([sys.executable, "-c", "import PyInstaller, cv2, numpy, serial, hid, tkinter"])
    build_root = safe_path(ROOT / "build", ROOT)
    safe_path(ROOT / "dist", ROOT)
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="native-", dir=build_root) as staging:
        stage = Path(staging)
        output = stage / "dist"
        run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean",
             "--distpath", str(output), "--workpath", str(stage / "work"),
             str(ROOT / "packaging" / "tos.spec")])
        (output / "platform-tools").mkdir()
        for tool, destination in artifacts(output).items():
            if tool != "tos_helper":
                (output / destination.name).rename(destination)
        validate_artifacts(output)
        # External icons are included with the installed application.
        shutil.copytree(ROOT / "src" / "tos_helper" / "assets", output / "assets")
        (output / ".tos-build.json").write_text(json.dumps({"platform": host(),
            "versions": {tool: metadata(tool)["version"] for tool in TOOLS}}, indent=2))
        replace_directory(output, ROOT / "dist")
    print(f"Build complete: {ROOT / 'dist'}")


def default_install_dir() -> Path:
    if host() == "win32":
        local = os.environ.get("LOCALAPPDATA")
        if not local:
            raise RuntimeError("LOCALAPPDATA is not defined.")
        return Path(local) / "Programs" / "TOS PC Tools"
    return Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local" / "share"))) / "tos-pc"


def validate_install_dir(target: Path) -> None:
    if not target.is_absolute() or ".." in target.parts:
        raise RuntimeError("Installation path must be absolute and contain no '..'.")
    resolved = target.resolve()
    # Protect source checkouts, build outputs and their ancestors.
    for protected in (ROOT, ROOT.parent / "linux-pc", ROOT.parent / "win-pc"):
        protected = protected.resolve()
        if resolved == protected or resolved in protected.parents or protected in resolved.parents:
            raise RuntimeError(f"Install directory must be independent of source: {target}")
    safe_path(target, target.parent)
    if target.exists():
        marker = target / INSTALL_MARKER
        if marker.is_symlink() or not marker.is_file():
            raise RuntimeError(f"Refusing to overwrite an unmanaged directory: {target}")
        if json.loads(marker.read_text(encoding="utf-8")).get("application") != "tos-pc":
            raise RuntimeError(f"Invalid TOS installation marker: {marker}")


def register_windows_shortcuts(target: Path, desktop: bool) -> None:
    # Pass data through JSON; paths never become executable PowerShell source.
    with tempfile.TemporaryDirectory(prefix="tos-shortcut-") as temp:
        config = Path(temp) / "shortcut.json"
        config.write_text(json.dumps({"target": str(artifacts(target)["tos_helper"]),
            "directory": str(target), "icon": str(target / "assets" / "typheye_rounded.ico"),
            "desktop": desktop}), encoding="utf-8")
        run(["powershell.exe", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
             "-File", str(ROOT / "scripts" / "shortcuts.ps1"), str(config)])


def install(directory: Path | None, desktop: bool) -> None:
    target = directory if directory is not None else default_install_dir()
    validate_install_dir(target)
    distribution = ROOT / "dist"
    manifest = json.loads((distribution / ".tos-build.json").read_text(encoding="utf-8"))
    if manifest["platform"] != host():
        raise RuntimeError("Distribution was built for a different platform; run build on this host.")
    validate_artifacts(distribution)
    for extension in ("png", "ico"):
        icon = distribution / "assets" / f"typheye_rounded.{extension}"
        if icon.is_symlink() or not icon.is_file():
            raise RuntimeError(f"Missing install resource: {icon}")
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".tos-install-", dir=target.parent) as temporary:
        staged = Path(temporary) / "application"
        shutil.copytree(distribution, staged)
        (staged / INSTALL_MARKER).write_text(json.dumps({"application": "tos-pc",
            "platform": host(), "versions": manifest["versions"]}, indent=2), encoding="utf-8")
        validate_artifacts(staged)
        replace_directory(staged, target)
    if host() == "win32":
        register_windows_shortcuts(target, desktop)
    print(f"Installed: {target}\nCLI tools: {target / 'platform-tools'}")


def clean(all_outputs: bool) -> None:
    targets = [ROOT / "build"]
    if all_outputs:
        targets.append(ROOT / "dist")
    for directory in (ROOT / "src", ROOT / "tests", ROOT / "scripts"):
        if directory.is_symlink():
            raise RuntimeError(f"Refusing symbolic link: {directory}")
        for base, dirs, _ in os.walk(directory, followlinks=False):
            for name in list(dirs):
                if name in ("__pycache__", ".pytest_cache"):
                    targets.append(Path(base) / name)
                    dirs.remove(name)
    # Preflight every deletion so an unsafe later path cannot cause partial cleanup.
    for target in targets:
        safe_path(target, ROOT)
        if target.is_dir():
            for base, dirs, files in os.walk(target, followlinks=False):
                for name in dirs + files:
                    safe_path(Path(base) / name, ROOT)
    for target in targets:
        remove_tree(target, ROOT)
    print("Clean complete; installed application and user configuration retained.")


def doctor() -> None:
    print(f"Platform: {host()}\nPython: {sys.executable}\nVersion: {sys.version}")
    print(f"Conda: {find_conda()}\nEnvironment: {os.environ.get('CONDA_DEFAULT_ENV', '(current)')}")
    for name in ("hidapi", "pyserial", "pyinstaller", "opencv-python", "pyreadline3"):
        try:
            print(f"{name}: {importlib.metadata.version(name)}")
        except importlib.metadata.PackageNotFoundError:
            print(f"{name}: not installed")
    run([sys.executable, "-c", "import tkinter; print('Tk:', tkinter.TkVersion)"])
    for tool, path in artifacts(ROOT / "dist").items():
        print(f"{tool} {metadata(tool)['version']}: {path} ({'exists' if path.exists() else 'not built'})")
    print(f"Install: {default_install_dir()}\nTDB configuration (unchanged): {Path.home() / '.tdb_device.json'}")


def main(argv: list[str] | None = None) -> int:
    original = list(sys.argv[1:] if argv is None else argv)
    # Accept --current-env before/after an action, but never consume app arguments after --.
    split = original.index("--") if "--" in original else len(original)
    current_env = "--current-env" in original[:split]
    parsed_argv = [a for a in original[:split] if a != "--current-env"] + original[split:]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--current-env", action="store_true", help="use the current Python, skip Conda routing")
    sub = parser.add_subparsers(dest="action", required=True)
    for action in ("setup", "build", "test", "doctor"):
        sub.add_parser(action)
    launch = sub.add_parser("run")
    launch.add_argument("--tool", choices=TOOLS, default="tos_helper")
    launch.add_argument("arguments", nargs=argparse.REMAINDER, help="arguments forwarded after --")
    installation = sub.add_parser("install")
    installation.add_argument("--install-dir", type=Path)
    if sys.platform == "win32":
        installation.add_argument("--desktop", action="store_true", help="also create a desktop shortcut")
    cleanup = sub.add_parser("clean")
    cleanup.add_argument("--all", action="store_true", help="also remove dist; keep installed app")
    args = parser.parse_args(parsed_argv)
    args.current_env = current_env
    host()
    routed = route_environment(args, original)
    if routed is not None:
        return routed
    if sys.version_info < (3, 11):
        raise RuntimeError("Python 3.11 or newer is required in the target environment.")
    if args.action == "setup":
        setup(current_env)
    elif args.action == "run":
        forwarded = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
        run([sys.executable, str(ROOT / "src" / args.tool / "app.py"), *forwarded])
    elif args.action == "build":
        build()
    elif args.action == "install":
        install(args.install_dir, getattr(args, "desktop", False))
    elif args.action == "clean":
        clean(args.all)
    elif args.action == "test":
        run([sys.executable, "-m", "unittest", "discover", "-s", str(ROOT / "tests"), "-v"])
    elif args.action == "doctor":
        doctor()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        print(f"Command failed (exit {exc.returncode}).", file=sys.stderr)
        raise SystemExit(exc.returncode if exc.returncode > 0 else 128 - exc.returncode)
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.TimeoutExpired) as exc:
        print(f"Error: {exc}", file=sys.stderr)
        raise SystemExit(1)
