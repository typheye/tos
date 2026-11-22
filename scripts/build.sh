#!/usr/bin/env bash
# Compatibility entry point; implementation lives in manage.py.
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1:-}" == '--install' || "${1:-}" == '-i' ]]; then
    shift
    bash "$SCRIPT_DIR/pc.sh" build "$@"
    exec bash "$SCRIPT_DIR/pc.sh" install "$@"
fi
exec bash "$SCRIPT_DIR/pc.sh" build "$@"
