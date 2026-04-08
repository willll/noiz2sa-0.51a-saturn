#!/usr/bin/env python3
"""Generate expected BulletML parser outputs from Saturn CD BLB files.

This script reads BLB files listed by LIST.TXT in BOSS/MIDDLE/ZAKO directories,
parses the binary tree directly (independent from SH2 parser implementation), and
writes a manifest used by SH2 tests to validate object parity on emulator/hardware.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

MAGIC = b"BLB\x00"
VERSION = 1
NONE_U32 = 0xFFFFFFFF

NODE_TYPE_NAMES = {
    0x00: "bulletml",
    0x01: "bullet",
    0x02: "action",
    0x03: "fire",
    0x04: "changeDirection",
    0x05: "changeSpeed",
    0x06: "accel",
    0x07: "wait",
    0x08: "repeat",
    0x09: "bulletRef",
    0x0A: "actionRef",
    0x0B: "fireRef",
    0x0C: "vanish",
    0x0D: "horizontal",
    0x0E: "vertical",
    0x0F: "term",
    0x10: "times",
    0x11: "direction",
    0x12: "speed",
    0x13: "param",
}

FNV_OFFSET = 2166136261
FNV_PRIME = 16777619


def fnv_update_byte(h: int, b: int) -> int:
    return ((h ^ (b & 0xFF)) * FNV_PRIME) & 0xFFFFFFFF


def fnv_update_u16(h: int, v: int) -> int:
    h = fnv_update_byte(h, v & 0xFF)
    h = fnv_update_byte(h, (v >> 8) & 0xFF)
    return h


def fnv_update_u32(h: int, v: int) -> int:
    h = fnv_update_byte(h, v & 0xFF)
    h = fnv_update_byte(h, (v >> 8) & 0xFF)
    h = fnv_update_byte(h, (v >> 16) & 0xFF)
    h = fnv_update_byte(h, (v >> 24) & 0xFF)
    return h


def fnv_update_str(h: int, s: str) -> int:
    for b in s.encode("ascii"):
        h = fnv_update_byte(h, b)
    # Terminator to avoid concatenation ambiguity.
    h = fnv_update_byte(h, 0)
    return h


def read_u16(data: bytes, off: int) -> tuple[int, int]:
    return struct.unpack_from("<H", data, off)[0], off + 2


def read_u32(data: bytes, off: int) -> tuple[int, int]:
    return struct.unpack_from("<I", data, off)[0], off + 4


def parse_string_table(data: bytes, off: int) -> list[str]:
    count, off = read_u32(data, off)
    out: list[str] = []
    for _ in range(count):
        n, off = read_u16(data, off)
        s = data[off : off + n].decode("ascii")
        off += n
        out.append(s)
    return out


def parse_node(data: bytes, off: int, strings: list[str]) -> tuple[int, int, int]:
    node_type, value_type, child_count, ref_id, value_id, label_id = struct.unpack_from(
        "<BBHIII", data, off
    )
    off += 16

    value = "" if value_id == NONE_U32 else strings[value_id]
    label = "" if label_id == NONE_U32 else strings[label_id]

    h = FNV_OFFSET
    h = fnv_update_str(h, NODE_TYPE_NAMES[node_type])
    h = fnv_update_byte(h, value_type)
    h = fnv_update_u16(h, child_count)
    h = fnv_update_u32(h, ref_id)
    h = fnv_update_str(h, value)
    h = fnv_update_str(h, label)

    node_count = 1
    top_actions = 0

    for _ in range(child_count):
        child_hash, child_count_val, child_top_actions, off = parse_node_with_top(data, off, strings)
        node_count += child_count_val
        top_actions += child_top_actions
        h = fnv_update_u32(h, child_hash)

    return h, node_count, off


def parse_node_with_top(data: bytes, off: int, strings: list[str]) -> tuple[int, int, int, int]:
    node_type = data[off]
    child_count = struct.unpack_from("<H", data, off + 2)[0]

    h, node_count, off = parse_node(data, off, strings)

    # Top-level actions are children of root; caller handles root-level counting.
    top_actions = 0
    if node_type == 0x02:  # action
        top_actions = 1

    # Re-derive children top-action count by replay is not needed here because parse_node
    # already consumed full subtree and only this node contributes to root-child criterion.
    if child_count:
        pass

    return h, node_count, top_actions, off


def parse_root(data: bytes) -> tuple[int, int, int, int]:
    if len(data) < 24:
        raise ValueError("BLB file too small")

    magic, version, orientation, _flags, str_off, _ref_off, tree_off, file_size = struct.unpack_from(
        "<4sHBBIIII", data, 0
    )
    if magic != MAGIC:
        raise ValueError(f"Invalid magic: {magic!r}")
    if version != VERSION:
        raise ValueError(f"Unsupported version: {version}")
    if file_size != len(data):
        raise ValueError(f"File size mismatch: header={file_size}, actual={len(data)}")

    strings = parse_string_table(data, str_off)

    # Parse root header first to count top-level actions among direct children.
    node_type, value_type, child_count, ref_id, value_id, label_id = struct.unpack_from(
        "<BBHIII", data, tree_off
    )
    if node_type != 0x00:
        raise ValueError(f"Root node type is not bulletml: {node_type}")

    value = "" if value_id == NONE_U32 else strings[value_id]
    label = "" if label_id == NONE_U32 else strings[label_id]

    h = FNV_OFFSET
    h = fnv_update_str(h, NODE_TYPE_NAMES[node_type])
    h = fnv_update_byte(h, value_type)
    h = fnv_update_u16(h, child_count)
    h = fnv_update_u32(h, ref_id)
    h = fnv_update_str(h, value)
    h = fnv_update_str(h, label)

    off = tree_off + 16
    node_count = 1
    top_actions = 0

    for _ in range(child_count):
        child_type = data[off]
        child_hash, child_nodes, off = parse_node(data, off, strings)
        if child_type == 0x02:
            top_actions += 1
        node_count += child_nodes
        h = fnv_update_u32(h, child_hash)

    return h, node_count, top_actions, orientation


def iter_listed_blb_files(data_root: Path) -> list[tuple[str, str, Path]]:
    out: list[tuple[str, str, Path]] = []
    for d in ("BOSS", "MIDDLE", "ZAKO"):
        list_path = data_root / d / "LIST.TXT"
        if not list_path.exists():
            raise FileNotFoundError(f"Missing list file: {list_path}")

        for raw in list_path.read_text(encoding="ascii", errors="strict").splitlines():
            line = raw.strip()
            if not line:
                continue
            file_path = data_root / d / line
            if not file_path.exists():
                raise FileNotFoundError(f"Missing BLB file listed in {list_path}: {line}")
            out.append((d, line, file_path))
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    entries = iter_listed_blb_files(args.data_root)

    lines = []
    for d, name, file_path in entries:
        data = file_path.read_bytes()
        h, node_count, top_actions, orientation = parse_root(data)
        lines.append(f"{d}|{name}|{h:08X}|{node_count}|{top_actions}|{orientation}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="ascii")

    print(f"Generated manifest: {args.output} ({len(lines)} entries)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
