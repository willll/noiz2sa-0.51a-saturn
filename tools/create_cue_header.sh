#!/bin/bash
# Create CUE file header for Sega Saturn CD image
# Usage: ./create_cue_header.sh <output_cue> <bin_filename>

OUTPUT_CUE="$1"
BIN_FILENAME="$2"

if [ -z "$OUTPUT_CUE" ] || [ -z "$BIN_FILENAME" ]; then
    echo "Usage: $0 <output_cue> <bin_filename>"
    exit 1
fi

# Create CUE file with proper formatting
{
    echo "FILE \"$BIN_FILENAME\" BINARY"
    echo "TRACK 01 MODE1/2352"
    echo "  INDEX 01 00:00:00"
} > "$OUTPUT_CUE"
