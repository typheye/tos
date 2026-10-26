#!/usr/bin/env bash
# Linux counterpart of flash.bat: build/sign then flash via OpenOCD.
# Flash addresses and sizes mirror flash.bat exactly -- keep both in sync.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.." || exit 2

MODE="${1:-system}"
OPENOCD="${OPENOCD:-openocd}"
OPENOCD_SPEED="${OPENOCD_SPEED:-50000}"

ROOT_OC="$(pwd -P)"
BUILD_DIR="$ROOT_OC/build/Release"
FLASH_DIR="$ROOT_OC/dist/flash"

fail() {
  echo "[ERROR] $*" >&2
  exit "${2:-4}"
}

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "[INFO] First-time setup: running cmake configure..."
  # --preset Release is mandatory: it is what supplies CMAKE_TOOLCHAIN_FILE
  # from CMakePresets.json. A bare `cmake -B` would pick the host compiler.
  cmake --preset Release || fail "CMake configure failed."
fi

command -v "$OPENOCD" >/dev/null 2>&1 || fail "OpenOCD not found: $OPENOCD" 2

echo "[INFO] Building factory_images..."
cmake --build "$BUILD_DIR" --target factory_images -- -j"$(nproc)" \
  || fail "Build/export failed."

require_file() {
  local f="$1"
  [ -f "$f" ] || {
    echo "[ERROR] Required artifact is missing: $f" >&2
    echo "" >&2
    echo "Build and export first:" >&2
    echo "  cmake --build build/Release --target factory_images" >&2
    exit 3
  }
  [ -s "$f" ] || fail "Required artifact is empty: $f" 3
}

run_openocd() {
  echo "[INFO] OpenOCD adapter speed: $OPENOCD_SPEED kHz"
  "$OPENOCD" \
    -f interface/cmsis-dap.cfg \
    -f target/stm32f4x.cfg \
    -c "adapter speed $OPENOCD_SPEED" \
    "$@"
}

flash_system() {
  require_file "$FLASH_DIR/system.elf"
  echo "[FLASH] SYSTEM $FLASH_DIR/system.elf"
  run_openocd \
    -c "init" \
    -c "reset halt" \
    -c "flash erase_address 0x08040000 0x000C0000" \
    -c "flash write_image {$FLASH_DIR/system.elf}" \
    -c "reset run" \
    -c "shutdown"
}

flash_runtime() {
  require_file "$FLASH_DIR/rec.elf"
  require_file "$FLASH_DIR/system.elf"
  echo "[FLASH] REC    $FLASH_DIR/rec.elf"
  echo "[FLASH] SYSTEM $FLASH_DIR/system.elf"
  run_openocd \
    -c "init" \
    -c "reset halt" \
    -c "flash erase_address 0x08010000 0x00010000" \
    -c "flash write_image {$FLASH_DIR/rec.elf}" \
    -c "flash erase_address 0x08040000 0x000C0000" \
    -c "flash write_image {$FLASH_DIR/system.elf}" \
    -c "reset run" \
    -c "shutdown"
}

flash_factory() {
  echo "[WARNING] Factory mode writes immutable ELF and the complete boot layout."
  require_file "$FLASH_DIR/factory.elf"
  require_file "$FLASH_DIR/sbl.elf"
  require_file "$FLASH_DIR/rec.elf"
  require_file "$FLASH_DIR/system.elf"
  echo "[FLASH] SBL    $FLASH_DIR/sbl.elf"
  echo "[FLASH] REC    $FLASH_DIR/rec.elf"
  echo "[FLASH] SYSTEM $FLASH_DIR/system.elf"
  echo "[FLASH] ELF    $FLASH_DIR/factory.elf"
  # Program ELF last so an interrupted migration cannot boot a partial new layout.
  run_openocd \
    -c "init" \
    -c "reset halt" \
    -c "flash erase_address 0x08004000 0x0000C000" \
    -c "flash write_image {$FLASH_DIR/sbl.elf}" \
    -c "flash erase_address 0x08010000 0x00010000" \
    -c "flash write_image {$FLASH_DIR/rec.elf}" \
    -c "flash erase_address 0x08040000 0x000C0000" \
    -c "flash write_image {$FLASH_DIR/system.elf}" \
    -c "flash write_image erase {$FLASH_DIR/factory.elf}" \
    -c "reset run" \
    -c "shutdown"
}

rc=0
case "$MODE" in
  system)  flash_system  || rc=$? ;;
  runtime) flash_runtime || rc=$? ;;
  factory) flash_factory || rc=$? ;;
  *)
    cat <<'EOF'
Usage: flash.sh [system|runtime|factory]

  system  - flash SYSTEM only (default)
  runtime - flash REC + SYSTEM
  factory - flash ELF + SBL + REC + SYSTEM
            Required once when migrating from the old boot layout.
EOF
    exit 2
    ;;
esac

if [ "$rc" -eq 0 ]; then
  echo ""
  echo "[OK] Upload completed successfully."
else
  echo ""
  echo "[ERROR] Upload failed with exit code $rc." >&2
fi
exit "$rc"
