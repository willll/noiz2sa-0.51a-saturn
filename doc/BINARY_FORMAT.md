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
| string   | variable     | Length-prefixed UTF-8 string         |

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
0x02   | length   | char[]  | data           | UTF-8 encoded string (not null-terminated)
```

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
