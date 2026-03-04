#!/bin/bash
# Run Noiz2sa in Mednafen Saturn emulator
# Usage: ./run_mednafen.sh [cue_file]
# Default: Uses BuildDrop/noiz2sa.cue if no parameter provided

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DROP="${PROJECT_ROOT}/BuildDrop"
DEFAULT_CUE="${BUILD_DROP}/noiz2sa.cue"

# Get CUE file from parameter or use default
CUE_FILE="${1:-$DEFAULT_CUE}"

# Validate CUE file exists
if [[ ! -f "$CUE_FILE" ]]; then
    echo "Error: CUE file not found: $CUE_FILE"
    echo "Usage: $0 [cue_file]"
    echo "Default: $DEFAULT_CUE"
    exit 1
fi

# Check if mednafen is installed
if ! command -v mednafen &> /dev/null; then
    echo "Error: mednafen not found in PATH"
    echo "Install mednafen: apt install mednafen"
    exit 1
fi

# Resolve absolute path for CUE file
CUE_FILE="$(cd "$(dirname "$CUE_FILE")" && pwd)/$(basename "$CUE_FILE")"

echo "Launching Mednafen with: $CUE_FILE"
echo "Emulator: Sega Saturn"
echo ""

# Run mednafen - will auto-detect system from CUE format
# Note: Settings like -ss.bios_jp should be configured in mednafen.cfg
exec mednafen -sound 0 -ss.cart debug -force_module ss "$CUE_FILE"
