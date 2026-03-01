#!/bin/bash
# Batch convert BulletML XML files to binary format
# Usage: ./convert_all.sh [input_dir] [output_dir]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY_CONVERTER="$SCRIPT_DIR/bulletml_converter.py"

# Default directories
INPUT_DIR="${1:-$SCRIPT_DIR/../../noiz2sa_share/boss}"
OUTPUT_DIR="${2:-$SCRIPT_DIR/../../noiz2sa_share/boss_binary}"

echo "BulletML Batch Converter"
echo "======================="
echo ""
echo "Input directory:  $INPUT_DIR"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Check if converter exists
if [ ! -f "$PY_CONVERTER" ]; then
    echo "Error: Python converter not found at $PY_CONVERTER"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Count files
total_files=$(find "$INPUT_DIR" -name "*.xml" | wc -l)
if [ "$total_files" -eq 0 ]; then
    echo "No XML files found in $INPUT_DIR"
    exit 1
fi

echo "Found $total_files XML files"
echo ""

# Convert each file
current=0
total_input_size=0
total_output_size=0

for xml_file in "$INPUT_DIR"/*.xml; do
    if [ -f "$xml_file" ]; then
        current=$((current + 1))
        filename=$(basename "$xml_file" .xml)
        output_file="$OUTPUT_DIR/$filename.bmlb"
        
        echo "[$current/$total_files] Converting $filename..."
        
        # Convert
        if python3 "$PY_CONVERTER" "$xml_file" "$output_file" > /dev/null 2>&1; then
            # Get file sizes
            input_size=$(stat -f%z "$xml_file" 2>/dev/null || stat -c%s "$xml_file" 2>/dev/null)
            output_size=$(stat -f%z "$output_file" 2>/dev/null || stat -c%s "$output_file" 2>/dev/null)
            
            total_input_size=$((total_input_size + input_size))
            total_output_size=$((total_output_size + output_size))
            
            compression=$(awk "BEGIN {printf \"%.1f\", ($output_size / $input_size) * 100}")
            echo "  ✓ $input_size → $output_size bytes ($compression%)"
        else
            echo "  ✗ Conversion failed!"
        fi
    fi
done

echo ""
echo "Conversion complete!"
echo "==================="
echo "Total files:       $current"
echo "Total input size:  $total_input_size bytes"
echo "Total output size: $total_output_size bytes"

if [ "$total_input_size" -gt 0 ]; then
    overall_compression=$(awk "BEGIN {printf \"%.1f\", ($total_output_size / $total_input_size) * 100}")
    saved=$((total_input_size - total_output_size))
    echo "Overall size:      $overall_compression% ($saved bytes saved)"
fi
