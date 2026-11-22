#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -n "${TOS_PYTHON:-}" ]]; then
    exec "$TOS_PYTHON" "$SCRIPT_DIR/manage.py" "$@"
elif command -v python3 >/dev/null 2>&1; then
    exec python3 "$SCRIPT_DIR/manage.py" "$@"
elif command -v python >/dev/null 2>&1; then
    exec python "$SCRIPT_DIR/manage.py" "$@"
elif command -v conda >/dev/null 2>&1; then
    exec conda run --no-capture-output -n base python "$SCRIPT_DIR/manage.py" "$@"
fi
printf '%s\n' 'Python 3.11+ or Conda is required.' >&2
exit 1
