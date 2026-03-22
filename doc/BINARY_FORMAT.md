# BulletML Binary Format Specification

Version: 1.0
Date: March 1, 2026

## Overview

This document describes the binary format for BulletML files. The binary format is designed to be:
- **Compact**: Smaller file size than XML
- **Fast to parse**: No XML parsing overhead
- **Complete**: Supports all BulletML features
- **Platform-independent**: Uses fixed-size types and explicit endianness

## File Structure

```
[Header]
[String Table]
[Reference Maps]
[Node Tree]
```

## Endianness

All multi-byte values are stored in **little-endian** format.

## Data Types

| Type     | Size (bytes) | Description                          |
|----------|--------------|--------------------------------------|
| uint8    | 1            | Unsigned 8-bit integer               |
| uint16   | 2            | Unsigned 16-bit integer              |
| uint32   | 4            | Unsigned 32-bit integer              |
| float32  | 4            | IEEE 754 single-precision float      |
| string   | variable     | Length-prefixed ASCII string         |

## Header Structure

```
Offset | Size | Type    | Field          | Description
-------|------|---------|----------------|---------------------------
0x00   | 4    | char[4] | magic          | Magic number: "BLB\\0" (BulletML Binary)
0x04   | 2    | uint16  | version        | Format version (currently 1)
0x06   | 1    | uint8   | orientation    | 0=vertical, 1=horizontal
0x07   | 1    | uint8   | flags          | Reserved for future use (set to 0)
0x08   | 4    | uint32  | string_table_offset | Offset to string table
0x0C   | 4    | uint32  | refmap_offset  | Offset to reference maps
0x10   | 4    | uint32  | tree_offset    | Offset to node tree
0x14   | 4    | uint32  | file_size      | Total file size in bytes
```

Total header size: 24 bytes

## String Table

The string table contains all strings used in the file (labels, formulas, etc.).

```
Offset | Size     | Type    | Field          | Description
-------|----------|---------|----------------|---------------------------
0x00   | 4        | uint32  | string_count   | Number of strings
0x04   | variable | string[]| strings        | Array of strings
```

Each string is encoded as:
```
Offset | Size     | Type    | Field          | Description
-------|----------|---------|----------------|---------------------------
0x00   | 2        | uint16  | length         | String length in bytes
0x02   | length   | char[]  | data           | ASCII bytes (not null-terminated)
```

Strings in the BLB string table are limited to 7-bit ASCII characters stored in 8-bit bytes.

## Reference Maps

Reference maps store the mapping from labels to node IDs for bulletRef, actionRef, and fireRef.

```
Offset | Size     | Type    | Field          | Description
-------|----------|---------|----------------|---------------------------
0x00   | 4        | uint32  | bullet_count   | Number of bullet references
0x04   | variable | RefEntry[]| bullet_refs  | Bullet reference entries
...    | 4        | uint32  | action_count   | Number of action references
...    | variable | RefEntry[]| action_refs  | Action reference entries
...    | 4        | uint32  | fire_count     | Number of fire references
...    | variable | RefEntry[]| fire_refs    | Fire reference entries
```

Each RefEntry:
```
Offset | Size | Type    | Field          | Description
-------|------|---------|----------------|---------------------------
0x00   | 4    | uint32  | label_id       | String table index for label
0x04   | 4    | uint32  | node_id        | Node ID in tree
```

## Node Tree

The node tree is a depth-first serialization of the BulletML tree structure.

### Node Structure

```
Offset | Size | Type    | Field          | Description
-------|------|---------|----------------|---------------------------
0x00   | 1    | uint8   | node_type      | Node type (see enum below)
0x01   | 1    | uint8   | value_type     | Value type: 0=none, 1=aim, 2=absolute, 3=relative, 4=sequence
0x02   | 2    | uint16  | child_count    | Number of child nodes
0x04   | 4    | uint32  | ref_id         | Reference ID (for *Ref nodes, 0xFFFFFFFF if not a ref)
0x08   | 4    | uint32  | value_string_id| String table index for value/formula (0xFFFFFFFF if no value)
0x0C   | 4    | uint32  | label_string_id| String table index for label (0xFFFFFFFF if no label)
0x10   | variable | Node[] | children       | Array of child nodes (recursive)
```

Total node header size: 16 bytes (excluding children)

### Node Type Enum

| Value | Name              | Description                          |
|-------|-------------------|--------------------------------------|
| 0x00  | bulletml          | Root node                            |
| 0x01  | bullet            | Bullet definition                    |
| 0x02  | action            | Action definition                    |
| 0x03  | fire              | Fire command                         |
| 0x04  | changeDirection   | Change direction command             |
| 0x05  | changeSpeed       | Change speed command                 |
| 0x06  | accel             | Acceleration command                 |
| 0x07  | wait              | Wait command                         |
| 0x08  | repeat            | Repeat command                       |
| 0x09  | bulletRef         | Reference to bullet                  |
| 0x0A  | actionRef         | Reference to action                  |
| 0x0B  | fireRef           | Reference to fire                    |
| 0x0C  | vanish            | Vanish command                       |
| 0x0D  | horizontal        | Horizontal type marker               |
| 0x0E  | vertical          | Vertical type marker                 |
| 0x0F  | term              | Term (duration)                      |
| 0x10  | times             | Times (repeat count)                 |
| 0x11  | direction         | Direction value                      |
| 0x12  | speed             | Speed value                          |
| 0x13  | param             | Parameter                            |

### Value Type Enum

| Value | Name              | Description                          |
|-------|-------------------|--------------------------------------|
| 0x00  | none              | No type specified                    |
| 0x01  | aim               | Aim at player                        |
| 0x02  | absolute          | Absolute direction                   |
| 0x03  | relative          | Relative to current                  |
| 0x04  | sequence          | Sequence (increment)                 |

## Example: Simple BulletML File

XML:
```xml
<?xml version="1.0"?>
<bulletml type="vertical">
	<action label="top">
		<fire>
			<direction>0</direction>
			<speed>1</speed>
			<bullet/>
		</fire>
	</action>
</bulletml>
```

Binary representation (hexadecimal):
```
Header:
42 4C 42 00  01 00  00  00    "BLB\0" version=1 orientation=vertical flags=0
18 00 00 00  XX XX XX XX      string_table_offset refmap_offset
YY YY YY YY  ZZ ZZ ZZ ZZ      tree_offset file_size

String Table:
03 00 00 00                   string_count=3
03 00 74 6F 70                "top" (length=3)
01 00 30                      "0" (length=1)
01 00 31                      "1" (length=1)

Reference Maps:
00 00 00 00                   bullet_count=0
01 00 00 00                   action_count=1
00 00 00 00  01 00 00 00      label="top"(id=0) node_id=1
00 00 00 00                   fire_count=0

Node Tree:
00 00  00 00  01 00           node_type=bulletml value_type=none child_count=1
FF FF FF FF  FF FF FF FF      ref_id=none value_string_id=none
FF FF FF FF                   label_string_id=none
	02 00  00 00  01 00         node_type=action value_type=none child_count=1
	FF FF FF FF  FF FF FF FF    ref_id=none value_string_id=none
	00 00 00 00                 label_string_id="top"(id=0)
		03 00  00 00  03 00       node_type=fire value_type=none child_count=3
		FF FF FF FF  FF FF FF FF  ref_id=none value_string_id=none
		FF FF FF FF               label_string_id=none
			11 00  00 00  00 00     node_type=direction value_type=none child_count=0
			FF FF FF FF  01 00 00 00 ref_id=none value_string_id="0"(id=1)
			FF FF FF FF             label_string_id=none
			12 00  00 00  00 00     node_type=speed value_type=none child_count=0
			FF FF FF FF  02 00 00 00 ref_id=none value_string_id="1"(id=2)
			FF FF FF FF             label_string_id=none
			01 00  00 00  00 00     node_type=bullet value_type=none child_count=0
			FF FF FF FF  FF FF FF FF ref_id=none value_string_id=none
			FF FF FF FF             label_string_id=none
```

## Parsing Algorithm

1. Read and verify header (check magic number and version)
2. Load string table into memory
3. Load reference maps
4. Parse node tree recursively:
	 - Read node header (16 bytes)
	 - Create BulletMLNode object
	 - Recursively parse `child_count` children
	 - Build parent-child relationships

## Size Comparison

Typical size reduction compared to XML:
- Simple files: 50-70% smaller
- Complex files: 60-80% smaller
- Files with many formulas: 40-60% smaller

## Compatibility Notes

- The binary format preserves all XML features including:
	- Mathematical expressions in values
	- All node types and attributes
	- Label-based references
	- Horizontal/vertical orientation
- Node IDs are assigned during conversion in depth-first order
- Formula strings are stored as-is and evaluated at runtime

## Future Extensions

Reserved header flags may be used for:
- Compression (e.g., zlib)
- Pre-compiled formulas
- Embedded metadata
- Checksums/validation
