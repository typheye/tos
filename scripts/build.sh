#!/usr/bin/env bash
# Build the TOS PC tools for Linux with PyInstaller.
set -euo pipefail

CLI_TOOLS=(tvidtool sbltool tdb)

INSTALL=0
for arg in "$@"; do
    case "$arg" in
        -i|--install) INSTALL=1 ;;
        -h|--help)
            echo "Usage: $(basename "$0") [--install]"
            echo "  --install   copy the built tools into the active Conda env's bin/"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 2
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build"
DIST_ROOT="$PROJECT_ROOT/dist"
PLATFORM_TOOLS="$DIST_ROOT/platform-tools"
SOURCE_ROOT="$PROJECT_ROOT/src"

# Re-exec inside the 'tos' conda environment when it is available. The
# re-exec guard prevents an infinite loop if conda does not export the env name.
if [ "${CONDA_DEFAULT_ENV:-}" != "tos" ] && [ "${TOS_BUILD_REEXEC:-0}" != "1" ]; then
    if command -v conda >/dev/null 2>&1; then
        echo "Activating 'tos' conda environment..."
        reexec_args=()
        [ "$INSTALL" -eq 1 ] && reexec_args=(--install)
        export TOS_BUILD_REEXEC=1
        exec conda run --no-capture-output -n tos bash "$0" ${reexec_args[@]+"${reexec_args[@]}"}
    fi
    echo "Conda environment 'tos' not active; using current Python." >&2
fi

if ! python -c "import PyInstaller, cv2, numpy, serial, hid" 2>/dev/null; then
    echo "Missing build dependency. Run: python -m pip install -r requirement.txt" >&2
    exit 1
fi

rm -rf "$BUILD_ROOT" "$DIST_ROOT"
mkdir -p "$PLATFORM_TOOLS"

for tool in tos_helper "${CLI_TOOLS[@]}"; do
    spec="$SOURCE_ROOT/$tool/$tool.spec"
    work="$BUILD_ROOT/pyinstaller/$tool"
    mkdir -p "$work"
    if [ "$tool" = "tos_helper" ]; then
        dist_path="$DIST_ROOT"
        binary="$DIST_ROOT/TOS Helper"
    else
        dist_path="$PLATFORM_TOOLS"
        binary="$PLATFORM_TOOLS/$tool"
    fi

    echo "python -m PyInstaller --noconfirm --clean --distpath $dist_path --workpath $work $spec"
    python -m PyInstaller \
        --noconfirm \
        --clean \
        --distpath "$dist_path" \
        --workpath "$work" \
        "$spec"

    if [ ! -x "$binary" ]; then
        echo "Expected output was not generated: $binary" >&2
        exit 1
    fi
    "$binary" --version
done

if [ "$INSTALL" -eq 1 ]; then
    if [ -z "${CONDA_PREFIX:-}" ]; then
        echo "No active Conda environment. Activate 'tos' before using --install." >&2
        exit 1
    fi
    mkdir -p "$CONDA_PREFIX/bin"
    for tool in "${CLI_TOOLS[@]}"; do
        install -m 0755 "$PLATFORM_TOOLS/$tool" "$CONDA_PREFIX/bin/$tool"
    done
    echo "Installed ${CLI_TOOLS[*]} to $CONDA_PREFIX/bin"
fi

echo
echo "Build complete:"
echo "  dist/TOS Helper"
for tool in "${CLI_TOOLS[@]}"; do
    echo "  dist/platform-tools/$tool"
done
