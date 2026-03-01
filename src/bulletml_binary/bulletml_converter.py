#!/usr/bin/env python3
"""BulletML XML -> BMLB binary converter.

This script converts BulletML XML files into the binary format documented in
src/bulletml_binary/BINARY_FORMAT.md.

Usage:
  python3 bulletml_converter.py input.xml output.bmlb
"""

from __future__ import annotations

import argparse
import io
import struct
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


MAGIC = b"BMLB"
VERSION = 1
NONE_U32 = 0xFFFFFFFF


NODE_TYPE: Dict[str, int] = {
    "bulletml": 0x00,
    "bullet": 0x01,
    "action": 0x02,
    "fire": 0x03,
    "changeDirection": 0x04,
    "changeSpeed": 0x05,
    "accel": 0x06,
    "wait": 0x07,
    "repeat": 0x08,
    "bulletRef": 0x09,
    "actionRef": 0x0A,
    "fireRef": 0x0B,
    "vanish": 0x0C,
    "horizontal": 0x0D,
    "vertical": 0x0E,
    "term": 0x0F,
    "times": 0x10,
    "direction": 0x11,
    "speed": 0x12,
    "param": 0x13,
}

VALUE_TYPE: Dict[str, int] = {
    "none": 0x00,
    "aim": 0x01,
    "absolute": 0x02,
    "relative": 0x03,
    "sequence": 0x04,
}


def _local_name(tag: str) -> str:
    if "}" in tag:
        return tag.split("}", 1)[1]
    return tag


@dataclass
class BinNode:
    name: str
    value_type: int = VALUE_TYPE["none"]
    ref_id: int = NONE_U32
    value_text: Optional[str] = None
    label_text: Optional[str] = None
    children: List["BinNode"] = field(default_factory=list)
    node_id: int = -1


class Converter:
    def __init__(self) -> None:
        self._strings: List[str] = []
        self._string_to_id: Dict[str, int] = {}

        # Domain IDs follow BulletMLParser behavior.
        self._domain_label_to_id: Dict[str, Dict[str, int]] = {
            "bullet": {},
            "action": {},
            "fire": {},
        }

        # Definition node for each domain ref id.
        self._domain_def_nodes: Dict[str, Dict[int, BinNode]] = {
            "bullet": {},
            "action": {},
            "fire": {},
        }

    def convert_file(self, in_path: Path, out_path: Path) -> None:
        root = ET.parse(in_path).getroot()
        root_node, is_horizontal = self._build_tree(root)

        self._assign_node_ids(root_node)
        self._collect_strings(root_node)

        out = io.BytesIO()
        out.write(b"\x00" * 24)  # Header placeholder.

        string_table_offset = out.tell()
        self._write_string_table(out)

        refmap_offset = out.tell()
        self._write_refmaps(out)

        tree_offset = out.tell()
        self._write_node_recursive(out, root_node)

        file_size = out.tell()
        self._write_header(out, is_horizontal, string_table_offset, refmap_offset, tree_offset, file_size)

        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(out.getvalue())

    def _build_tree(self, xml_root: ET.Element) -> Tuple[BinNode, bool]:
        root_tag = _local_name(xml_root.tag)
        if root_tag != "bulletml":
            raise ValueError(f"Root element must be <bulletml>, got <{root_tag}>")

        orientation = xml_root.attrib.get("type", "vertical").strip().lower()
        is_horizontal = orientation == "horizontal"

        root_node = self._build_node(xml_root)
        return root_node, is_horizontal

    def _build_node(self, elem: ET.Element) -> BinNode:
        name = _local_name(elem.tag)
        if name not in NODE_TYPE:
            raise ValueError(f"Unsupported BulletML node <{name}>")

        node = BinNode(name=name)

        # Parse attributes matching BulletML parser behavior.
        type_attr = elem.attrib.get("type")
        if type_attr is not None and name != "bulletml":
            t = type_attr.strip()
            if t not in VALUE_TYPE:
                raise ValueError(f"Unsupported type attribute '{t}' on <{name}>")
            node.value_type = VALUE_TYPE[t]

        label_attr = elem.attrib.get("label")
        if label_attr is not None:
            label = label_attr.strip()
            node.label_text = label
            domain = self._domain_for_label(name)
            node.ref_id = self._get_or_create_domain_id(domain, label)
            if name in ("bullet", "action", "fire"):
                self._domain_def_nodes[name][node.ref_id] = node

        # TinyXML parser stores first text node as value for this element.
        if elem.text is not None:
            txt = elem.text.strip()
            if txt:
                node.value_text = txt

        for child in list(elem):
            if not isinstance(child.tag, str):
                continue
            node.children.append(self._build_node(child))

        return node

    @staticmethod
    def _domain_for_label(node_name: str) -> str:
        if node_name == "bulletRef":
            return "bullet"
        if node_name == "actionRef":
            return "action"
        if node_name == "fireRef":
            return "fire"
        return node_name

    def _get_or_create_domain_id(self, domain: str, label: str) -> int:
        if domain not in self._domain_label_to_id:
            # Only bullet/action/fire are valid domains in BulletML refs.
            self._domain_label_to_id[domain] = {}

        domain_map = self._domain_label_to_id[domain]
        if label in domain_map:
            return domain_map[label]

        new_id = len(domain_map)
        domain_map[label] = new_id
        return new_id

    def _assign_node_ids(self, root: BinNode) -> None:
        next_id = 0

        def walk(n: BinNode) -> None:
            nonlocal next_id
            n.node_id = next_id
            next_id += 1
            for c in n.children:
                walk(c)

        walk(root)

    def _add_string(self, s: str) -> int:
        if s in self._string_to_id:
            return self._string_to_id[s]
        idx = len(self._strings)
        self._strings.append(s)
        self._string_to_id[s] = idx
        return idx

    def _collect_strings(self, root: BinNode) -> None:
        def walk(n: BinNode) -> None:
            if n.label_text:
                self._add_string(n.label_text)
            if n.value_text:
                self._add_string(n.value_text)
            for c in n.children:
                walk(c)

        walk(root)

    def _write_header(
        self,
        out: io.BytesIO,
        is_horizontal: bool,
        string_table_offset: int,
        refmap_offset: int,
        tree_offset: int,
        file_size: int,
    ) -> None:
        header = struct.pack(
            "<4sHBBIIII",
            MAGIC,
            VERSION,
            1 if is_horizontal else 0,
            0,
            string_table_offset,
            refmap_offset,
            tree_offset,
            file_size,
        )
        out.seek(0)
        out.write(header)
        out.seek(file_size)

    def _write_string_table(self, out: io.BytesIO) -> None:
        out.write(struct.pack("<I", len(self._strings)))
        for s in self._strings:
            raw = s.encode("utf-8")
            if len(raw) > 0xFFFF:
                raise ValueError(f"String too long for format (max 65535 bytes): {s[:80]}...")
            out.write(struct.pack("<H", len(raw)))
            out.write(raw)

    def _make_ref_entries(self, domain: str) -> List[Tuple[int, int]]:
        label_map = self._domain_label_to_id.get(domain, {})
        def_nodes = self._domain_def_nodes.get(domain, {})

        entries: List[Tuple[int, int]] = []
        for label, ref_id in sorted(label_map.items(), key=lambda kv: kv[1]):
            label_id = self._string_to_id.get(label, NONE_U32)
            node = def_nodes.get(ref_id)
            node_id = node.node_id if node is not None else NONE_U32
            entries.append((label_id, node_id))
        return entries

    def _write_refmaps(self, out: io.BytesIO) -> None:
        for domain in ("bullet", "action", "fire"):
            entries = self._make_ref_entries(domain)
            out.write(struct.pack("<I", len(entries)))
            for label_id, node_id in entries:
                out.write(struct.pack("<II", label_id, node_id))

    def _write_node_recursive(self, out: io.BytesIO, node: BinNode) -> None:
        node_type = NODE_TYPE[node.name]
        value_string_id = NONE_U32
        label_string_id = NONE_U32

        if node.value_text:
            value_string_id = self._string_to_id[node.value_text]
        if node.label_text:
            label_string_id = self._string_to_id[node.label_text]

        out.write(
            struct.pack(
                "<BBHIII",
                node_type,
                node.value_type,
                len(node.children),
                node.ref_id,
                value_string_id,
                label_string_id,
            )
        )

        for child in node.children:
            self._write_node_recursive(out, child)


def _parse_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert BulletML XML files to .bmlb binary format"
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input XML file or directory containing XML files",
    )
    parser.add_argument(
        "output",
        type=Path,
        help="Output .bmlb file (file mode) or output directory (directory mode)",
    )
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    args = _parse_args(argv)

    if not args.input.exists():
        print(f"error: input file not found: {args.input}", file=sys.stderr)
        return 1

    # Directory mode: convert all *.xml files in input directory.
    if args.input.is_dir():
        if args.output.exists() and not args.output.is_dir():
            print(
                f"error: output must be a directory when input is a directory: {args.output}",
                file=sys.stderr,
            )
            return 1

        args.output.mkdir(parents=True, exist_ok=True)
        xml_files = sorted(args.input.glob("*.xml"))
        if not xml_files:
            print(f"error: no XML files found in directory: {args.input}", file=sys.stderr)
            return 1

        success = 0
        failed = 0
        for xml_path in xml_files:
            out_path = args.output / f"{xml_path.stem}.bmlb"
            try:
                conv = Converter()
                conv.convert_file(xml_path, out_path)
                success += 1
                print(f"Converted: {xml_path} -> {out_path}")
            except Exception as exc:  # noqa: BLE001
                failed += 1
                print(f"error: conversion failed for {xml_path}: {exc}", file=sys.stderr)

        print(f"Summary: {success} succeeded, {failed} failed")
        return 0 if failed == 0 else 1

    # Single file mode.
    try:
        conv = Converter()
        conv.convert_file(args.input, args.output)
    except Exception as exc:  # noqa: BLE001
        print(f"error: conversion failed: {exc}", file=sys.stderr)
        return 1

    print(f"Converted: {args.input} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
