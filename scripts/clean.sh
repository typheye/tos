#!/usr/bin/env bash
# Remove build artifacts, dist output and Python caches.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

rm -rf build dist
find . -type d -name __pycache__ -prune -exec rm -rf {} +

echo "Clean complete."
