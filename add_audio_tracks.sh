#!/bin/bash
# Audio track generation script for Sega Saturn CD images
# Converts audio files to raw PCM and appends them to BIN/CUE
# Based on /saturn/SaturnRingLib/saturnringlib/shared.mk

set -e

BUILD_BIN="$1"
BUILD_CUE="$2"
MUSIC_DIR="$3"
CD_NAME="$4"

if [ -z "$BUILD_BIN" ] || [ -z "$BUILD_CUE" ] || [ -z "$MUSIC_DIR" ] || [ -z "$CD_NAME" ]; then
    echo "Usage: $0 <BUILD_BIN> <BUILD_CUE> <MUSIC_DIR> <CD_NAME>"
    exit 1
fi

# Check if sox is available
if ! command -v sox &> /dev/null; then
    echo "Warning: sox not found - audio track generation skipped"
    echo "Install sox to enable audio track support: sudo apt-get install sox"
    exit 0
fi

# Function to convert audio file to raw PCM and sector-align
convert_audio_to_raw() {
    local audiofile="$1"
    local rawfile="$2"
    local filter_option="$3"
    
    if [ ! -f "$rawfile" ] || [ "$audiofile" -nt "$rawfile" ]; then
        if [ -f "$rawfile" ]; then
            echo "Regenerating $rawfile (source file is newer)"
        else
            echo "Converting $audiofile to $rawfile"
        fi
        
        # Convert to raw PCM: 44.1kHz, 16-bit signed integer, stereo
        if [ -n "$filter_option" ]; then
            filter_var="SRL_SOX_FILTERS_$filter_option"
            filter_cmd=$(eval echo \$$filter_var)
            if [ -n "$filter_cmd" ]; then
                sox "$audiofile" -t raw -r 44100 -e signed-integer -b 16 -c 2 "$rawfile" $filter_cmd
            else
                echo "Warning: No SOX_FILTERS_$filter_option defined, using no filters"
                sox "$audiofile" -t raw -r 44100 -e signed-integer -b 16 -c 2 "$rawfile"
            fi
        else
            sox "$audiofile" -t raw -r 44100 -e signed-integer -b 16 -c 2 "$rawfile"
        fi
        
        # Sector-align to 2352 bytes (CD-DA sector size)
        size=$(stat -c%s "$rawfile")
        target_sectors=$((size / 2352))
        if [ $((size % 2352)) -ne 0 ]; then
            target_sectors=$((target_sectors + 1))
        fi
        target_size=$((target_sectors * 2352))
        
        if [ $size -lt $target_size ]; then
            padding_needed=$((target_size - size))
            # Pad with zeros to reach sector boundary
            dd if=/dev/zero of="$rawfile" bs=1 count=1 seek=$((target_size - 1)) 2>/dev/null
        fi
        
        echo "Converted $audiofile to $rawfile (${size} -> ${target_size} bytes, ${target_sectors} sectors)"
    else
        echo "Using existing $rawfile (up to date)"
    fi
}

# Start processing
track=2
total_size=$(stat -c%s "$BUILD_BIN")
sectors=$((total_size / 2352))
echo "Starting with ${total_size} bytes (${sectors} sectors)"

# Create temp file for audio list
AUDIO_TEMP=$(mktemp)
trap "rm -f $AUDIO_TEMP" EXIT

# Find audio files and convert them to raw
if [ -f "$MUSIC_DIR/tracklist" ]; then
    echo "Using tracklist: $MUSIC_DIR/tracklist"
    # Parse tracklist and handle files with spaces
    # Ensure file ends with newline, then filter
    ( cat "$MUSIC_DIR/tracklist"; echo ) | sed 's/^\s*//;s/\s*$//;/^$/d;/^#/d' | \
    while IFS= read -r line; do
        # Extract filename part before colon (if any)
        if echo "$line" | grep -q ':'; then
            audiofile=${line%%:*}
        else
            audiofile=$line
        fi
        
        # Handle absolute paths vs relative paths
        if [[ "$audiofile" == /* ]]; then
            # Absolute path - use as is
            :
        elif [[ "$audiofile" == */* ]]; then
            # Relative path with directory - use as is relative to current dir
            :
        else
            # Just filename - prepend MUSIC_DIR
            audiofile="$MUSIC_DIR/$audiofile"
        fi
        
        rawfile="$audiofile.raw"
        
        if [ -f "$audiofile" ]; then
            # Get filter option for this file
            filter_option=""
            if echo "$line" | grep -q ':'; then
                filter_option=${line#*:}
            fi
            
            convert_audio_to_raw "$audiofile" "$rawfile" "$filter_option"
            echo "$rawfile" >> "$AUDIO_TEMP"
        else
            echo "Warning: Audio file not found: $audiofile"
        fi
    done
else
    echo "No tracklist found, auto-discovering audio files in $MUSIC_DIR"
    # Auto-discover audio files and convert to raw
    find "$MUSIC_DIR" \( -name '*.mp3' -o -name '*.wav' -o -name '*.ogg' -o -name '*.flac' -o -name '*.aac' -o -name '*.m4a' -o -name '*.wma' \) -type f | sort | \
    while IFS= read -r audiofile; do
        rawfile="$audiofile.raw"
        convert_audio_to_raw "$audiofile" "$rawfile" ""
        echo "$rawfile" >> "$AUDIO_TEMP"
    done
fi

# Check if we found any audio files
if [ ! -s "$AUDIO_TEMP" ]; then
    echo "No audio files found - skipping audio track generation"
    exit 0
fi

echo "Adding audio tracks to BIN/CUE..."

# Process each audio track
while IFS= read -r rawfile; do
    [ -z "$rawfile" ] && continue
    [ ! -f "$rawfile" ] && continue
    
    echo "Track $track: starts at sector $sectors"
    
    # Add TRACK entry to CUE
    printf "  TRACK %02d AUDIO\n" $track >> "$BUILD_CUE"
    
    # 150 frames (2 seconds) pregap required for first audio track after data
    if [ $track -eq 2 ]; then
        echo "    PREGAP 00:02:00" >> "$BUILD_CUE"
    fi
    
    # Calculate INDEX time in MSF format (MM:SS:FF)
    index_sectors=$sectors
    minutes=$((index_sectors / (60 * 75)))
    remaining=$((index_sectors % (60 * 75)))
    seconds=$((remaining / 75))
    frames=$((remaining % 75))
    
    # Validate frame count
    if [ $frames -ge 75 ]; then
        echo "ERROR: Invalid frame count $frames (must be 0-74)"
        echo "This indicates sector misalignment in the audio file(s)"
        exit 1
    fi
    
    msf=$(printf "%02d:%02d:%02d" $minutes $seconds $frames)
    echo "    INDEX 01 $msf" >> "$BUILD_CUE"
    echo "  INDEX calculation: sector $index_sectors = $msf"
    
    # Verify sector alignment
    size=$(stat -c%s "$rawfile")
    if [ $((size % 2352)) -ne 0 ]; then
        echo "ERROR: File $rawfile is not sector-aligned ($size bytes)"
        echo "File size must be a multiple of 2352 bytes"
        exit 1
    fi
    
    sectors_in_file=$((size / 2352))
    echo "  Adding $rawfile: $size bytes ($sectors_in_file sectors)"
    
    # Append raw audio to BIN file
    cat "$rawfile" >> "$BUILD_BIN"
    
    total_size=$((total_size + size))
    sectors=$((sectors + sectors_in_file))
    echo "  New total: $total_size bytes ($((total_size / 2352)) sectors)"
    
    track=$((track + 1))
done < "$AUDIO_TEMP"

echo "Audio track generation complete: $((track - 2)) tracks added"
