#!/bin/bash
# Run a Saturn CUE image in Kronos.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CUE_FILE="${PROJECT_ROOT}/BuildDrop/noiz2sa.cue"

if [[ ! -f "$CUE_FILE" ]]; then
    echo "Error: CUE file not found: $CUE_FILE"
    exit 1
fi

if ! command -v kronos &> /dev/null; then
    echo "Error: kronos not found in PATH"
    exit 1
fi

echo "Launching Kronos with: $CUE_FILE"
exec env QT_QPA_PLATFORM="xcb" kronos -a "$CUE_FILE"
