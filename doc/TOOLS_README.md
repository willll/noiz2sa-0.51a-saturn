# Noiz2sa Tools

This directory contains utility tools for the Noiz2sa Saturn port.

## BulletML Converter

The `bulletml_converter.py` script provides bidirectional conversion between BulletML XML and binary formats.

### Features

- **XML → Binary**: Convert `.xml` BulletML files to compact `.blb` binary format
- **Binary → XML**: Decompile `.blb` files back to human-readable XML
- **Batch Processing**: Convert entire directories of files at once
- **Automatic Detection**: Conversion direction is automatically detected from file extensions
- **Format Validation**: Validates both input and output formats

### Binary Format Benefits

- **40-80% smaller** than equivalent XML files
- **Faster loading** - no XML parsing overhead
- **Platform-independent** - explicit endianness and fixed-size types
- **Complete** - supports all BulletML features

### Usage

#### Single File Conversion

```bash
# XML to Binary
python3 tools/bulletml_converter.py input.xml output.blb

# Binary to XML
python3 tools/bulletml_converter.py input.blb output.xml
```

#### Batch Directory Conversion

```bash
# Convert all XML files in a directory to binary
python3 tools/bulletml_converter.py noiz2sa_share/boss/ binary_output/

# Convert all binary files in a directory back to XML
python3 tools/bulletml_converter.py binary_output/ xml_output/
```

#### Direct Execution

The script is executable and can be run directly:

```bash
./tools/bulletml_converter.py input.xml output.bmlb
```

### Examples

```bash
# Convert a single boss pattern
./tools/bulletml_converter.py noiz2sa_share/boss/88way.xml patterns/88way.blb

# Roundtrip test (XML → Binary → XML)
./tools/bulletml_converter.py noiz2sa_share/boss/88way.xml /tmp/88way.blb
./tools/bulletml_converter.py /tmp/88way.blb /tmp/88way_restored.xml

# Batch convert all boss patterns
./tools/bulletml_converter.py noiz2sa_share/boss/ binary_patterns/

# Batch convert middle stage patterns
./tools/bulletml_converter.py noiz2sa_share/middle/ binary_patterns/
```

### File Format

See [`BINARY_FORMAT.md`](BINARY_FORMAT.md) for complete binary format specification.

### Requirements

- Python 3.6 or higher
- Standard library only (no external dependencies)

### Error Handling

The converter validates input files and provides clear error messages:

```bash
$ python3 tools/bulletml_converter.py invalid.txt output.blb
error: Cannot determine conversion mode from extensions: .txt -> .blb
Supported conversions: .xml -> .blb or .blb -> .xml
```

### Performance

Typical conversion times on Saturn development machine:

- Single file: < 10ms
- Directory (30 files): < 200ms
- Large pattern (10KB XML): < 5ms

### Integration

To use binary BulletML files in your Saturn port:

1. Convert XML patterns to `.blb` format
2. Place binary files in `cd/data/` directory
3. Use `BulletMLParserBin` instead of `BulletMLParserBLB`
4. Load patterns from binary files

See `src/bulletml_binary/README.md` for C++ integration details.
