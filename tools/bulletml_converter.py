#!/usr/bin/env python3
"""BulletML bidirectional converter: XML <-> BLB binary format.

This script converts BulletML files between XML and binary formats:
- XML -> BLB: Converts BulletML XML files to binary format
- BLB -> XML: Decompiles binary BulletML files back to XML

Usage:
  python3 bulletml_converter.py input.xml output.blb    # XML to binary
  python3 bulletml_converter.py input.blb output.xml    # Binary to XML
  python3 bulletml_converter.py input_dir/ output_dir/   # Batch conversion
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
from xml.dom import minidom


MAGIC = b"BLB\x00"
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

NODE_TYPE_NAMES: Dict[int, str] = {v: k for k, v in NODE_TYPE.items()}

VALUE_TYPE: Dict[str, int] = {
    "none": 0x00,
    "aim": 0x01,
    "absolute": 0x02,
    "relative": 0x03,
    "sequence": 0x04,
}

VALUE_TYPE_NAMES: Dict[int, str] = {v: k for k, v in VALUE_TYPE.items()}


def _local_name(tag: str) -> str:
    """Extract local name from potentially namespaced tag."""
    if "}" in tag:
        return tag.split("}", 1)[1]
    return tag


@dataclass
class BinNode:
    """Intermediate representation of a BulletML node."""
    name: str
    value_type: int = VALUE_TYPE["none"]
    ref_id: int = NONE_U32
    value_text: Optional[str] = None
    label_text: Optional[str] = None
    children: List["BinNode"] = field(default_factory=list)
    node_id: int = -1


class Converter:
    """Converts BulletML XML to binary format."""

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
        """Convert XML file to binary format (.blb)."""
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
        """Build intermediate tree from XML root."""
        root_tag = _local_name(xml_root.tag)
        if root_tag != "bulletml":
            raise ValueError(f"Root element must be <bulletml>, got <{root_tag}>")

        orientation = xml_root.attrib.get("type", "vertical").strip().lower()
        is_horizontal = orientation == "horizontal"

        root_node = self._build_node(xml_root)
        return root_node, is_horizontal

    def _build_node(self, elem: ET.Element) -> BinNode:
        """Build a node from XML element."""
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
        """Get domain name for labeled node."""
        if node_name == "bulletRef":
            return "bullet"
        if node_name == "actionRef":
            return "action"
        if node_name == "fireRef":
            return "fire"
        return node_name

    def _get_or_create_domain_id(self, domain: str, label: str) -> int:
        """Get or create a domain ID for a label."""
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
        """Assign sequential node IDs to tree."""
        next_id = 0

        def walk(n: BinNode) -> None:
            nonlocal next_id
            n.node_id = next_id
            next_id += 1
            for c in n.children:
                walk(c)

        walk(root)

    def _add_string(self, s: str) -> int:
        """Add string to table and return its ID."""
        if s in self._string_to_id:
            return self._string_to_id[s]
        idx = len(self._strings)
        self._strings.append(s)
        self._string_to_id[s] = idx
        return idx

    def _collect_strings(self, root: BinNode) -> None:
        """Collect all strings from tree."""
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
        """Write binary file header."""
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
        """Write string table to output."""
        out.write(struct.pack("<I", len(self._strings)))
        for s in self._strings:
            try:
                raw = s.encode("ascii")
            except UnicodeEncodeError as exc:
                raise ValueError(
                    f"BLB string table only supports ASCII text: {s[:80]}..."
                ) from exc
            if len(raw) > 0xFFFF:
                raise ValueError(f"String too long for format (max 65535 bytes): {s[:80]}...")
            out.write(struct.pack("<H", len(raw)))
            out.write(raw)

    def _make_ref_entries(self, domain: str) -> List[Tuple[int, int]]:
        """Create reference map entries for a domain."""
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
        """Write reference maps to output."""
        for domain in ("bullet", "action", "fire"):
            entries = self._make_ref_entries(domain)
            out.write(struct.pack("<I", len(entries)))
            for label_id, node_id in entries:
                out.write(struct.pack("<II", label_id, node_id))

    def _write_node_recursive(self, out: io.BytesIO, node: BinNode) -> None:
        """Write node and its children recursively."""
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


class Deconverter:
    """Converts binary BulletML format back to XML."""

    def __init__(self) -> None:
        self._strings: List[str] = []
        self._bullet_refs: Dict[int, str] = {}
        self._action_refs: Dict[int, str] = {}
        self._fire_refs: Dict[int, str] = {}

    def convert_file(self, in_path: Path, out_path: Path) -> None:
        """Convert binary file (BLB) to XML format."""
        data = in_path.read_bytes()
        
        # Parse header
        if len(data) < 24:
            raise ValueError("File too small to be valid BLB")
        
        magic, version, orientation, flags, str_offset, ref_offset, tree_offset, file_size = struct.unpack(
            "<4sHBBIIII", data[:24]
        )
        
        if magic != MAGIC:
            raise ValueError(f"Invalid magic number: expected {MAGIC}, got {magic}")
        
        if version != VERSION:
            raise ValueError(f"Unsupported version: {version}")
        
        is_horizontal = orientation == 1
        
        # Read string table
        self._read_string_table(data, str_offset)
        
        # Read reference maps
        self._read_refmaps(data, ref_offset)
        
        # Read node tree
        root_node, _ = self._read_node(data, tree_offset)
        
        # Build XML
        xml_root = self._build_xml(root_node, is_horizontal)
        
        # Write formatted XML
        xml_str = ET.tostring(xml_root, encoding="utf-8")
        dom = minidom.parseString(xml_str)
        pretty_xml = dom.toprettyxml(indent="  ", encoding="utf-8")
        
        # Remove extra blank lines
        lines = pretty_xml.decode("utf-8").split("\n")
        lines = [line for line in lines if line.strip()]
        
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def _read_string_table(self, data: bytes, offset: int) -> None:
        """Read string table from binary data."""
        string_count, = struct.unpack("<I", data[offset:offset+4])
        pos = offset + 4
        
        for _ in range(string_count):
            length, = struct.unpack("<H", data[pos:pos+2])
            pos += 2
            try:
                string_data = data[pos:pos+length].decode("ascii")
            except UnicodeDecodeError as exc:
                raise ValueError("BLB string table contains non-ASCII bytes") from exc
            pos += length
            self._strings.append(string_data)

    def _read_refmaps(self, data: bytes, offset: int) -> None:
        """Read reference maps from binary data."""
        pos = offset
        
        # Read bullet refs
        bullet_count, = struct.unpack("<I", data[pos:pos+4])
        pos += 4
        for _ in range(bullet_count):
            label_id, node_id = struct.unpack("<II", data[pos:pos+8])
            pos += 8
            if label_id != NONE_U32:
                self._bullet_refs[node_id] = self._strings[label_id]
        
        # Read action refs
        action_count, = struct.unpack("<I", data[pos:pos+4])
        pos += 4
        for _ in range(action_count):
            label_id, node_id = struct.unpack("<II", data[pos:pos+8])
            pos += 8
            if label_id != NONE_U32:
                self._action_refs[node_id] = self._strings[label_id]
        
        # Read fire refs
        fire_count, = struct.unpack("<I", data[pos:pos+4])
        pos += 4
        for _ in range(fire_count):
            label_id, node_id = struct.unpack("<II", data[pos:pos+8])
            pos += 8
            if label_id != NONE_U32:
                self._fire_refs[node_id] = self._strings[label_id]

    def _read_node(self, data: bytes, offset: int) -> Tuple[BinNode, int]:
        """Read a node and return it with the next offset."""
        node_type, value_type, child_count, ref_id, value_str_id, label_str_id = struct.unpack(
            "<BBHIII", data[offset:offset+16]
        )
        
        node_name = NODE_TYPE_NAMES.get(node_type, f"unknown_{node_type}")
        node = BinNode(
            name=node_name,
            value_type=value_type,
            ref_id=ref_id if ref_id != NONE_U32 else NONE_U32,
        )
        
        if value_str_id != NONE_U32:
            node.value_text = self._strings[value_str_id]
        
        if label_str_id != NONE_U32:
            node.label_text = self._strings[label_str_id]
        
        pos = offset + 16
        
        # Read children
        for _ in range(child_count):
            child_node, pos = self._read_node(data, pos)
            node.children.append(child_node)
        
        return node, pos

    def _build_xml(self, node: BinNode, is_horizontal: bool) -> ET.Element:
        """Build XML element from intermediate node."""
        elem = ET.Element(node.name)
        
        # Add type attribute for bulletml root
        if node.name == "bulletml":
            if is_horizontal:
                elem.set("type", "horizontal")
            else:
                elem.set("type", "vertical")
        
        # Add type attribute for value types
        if node.value_type != VALUE_TYPE["none"] and node.name != "bulletml":
            type_name = VALUE_TYPE_NAMES.get(node.value_type, "none")
            if type_name != "none":
                elem.set("type", type_name)
        
        # Add label attribute
        if node.label_text:
            elem.set("label", node.label_text)
        
        # Add value text
        if node.value_text:
            elem.text = node.value_text
        
        # Build children
        for i, child in enumerate(node.children):
            child_elem = self._build_xml(child, is_horizontal)
            elem.append(child_elem)
            # Add newlines for readability
            if i == 0:
                elem.text = (elem.text or "") + "\n  "
            child_elem.tail = "\n  " if i < len(node.children) - 1 else "\n"
        
        return elem


def _parse_args(argv: List[str]) -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(
        description="Convert BulletML files between XML and binary formats"
    )
    parser.add_argument(
        "input",
        type=Path,
        help="Input file (.xml or .blb) or directory",
    )
    parser.add_argument(
        "output",
        type=Path,
        help="Output file or directory",
    )
    parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Overwrite existing files without prompting",
    )
    return parser.parse_args(argv)


def _detect_conversion_mode(input_path: Path, output_path: Path) -> str:
    """Detect conversion mode based on file extensions."""
    input_ext = input_path.suffix.lower()
    output_ext = output_path.suffix.lower()
    
    if input_ext == ".xml" and output_ext == ".blb":
        return "xml_to_bin"
    elif input_ext == ".blb" and output_ext == ".xml":
        return "bin_to_xml"
    elif input_path.is_dir():
        # Auto-detect based on files in directory
        if any(input_path.glob("*.xml")):
            return "xml_to_bin"
        elif any(input_path.glob("*.blb")):
            return "bin_to_xml"
    
    raise ValueError(
        f"Cannot determine conversion mode from extensions: {input_ext} -> {output_ext}\n"
        "Supported conversions: .xml -> .blb or .blb -> .xml"
    )


def main(argv: List[str]) -> int:
    """Main entry point."""
    args = _parse_args(argv)

    if not args.input.exists():
        print(f"error: input file not found: {args.input}", file=sys.stderr)
        return 1

    # Directory mode
    if args.input.is_dir():
        if args.output.exists() and not args.output.is_dir():
            print(
                f"error: output must be a directory when input is a directory: {args.output}",
                file=sys.stderr,
            )
            return 1

        args.output.mkdir(parents=True, exist_ok=True)
        
        # Detect conversion mode
        try:
            mode = _detect_conversion_mode(args.input, args.output)
        except ValueError as e:
            print(f"error: {e}", file=sys.stderr)
            return 1
        
        if mode == "xml_to_bin":
            pattern = "*.xml"
            suffix = ".blb"
        else:
            pattern = "*.blb"
            suffix = ".xml"
        
        input_files = sorted(args.input.glob(pattern))
        if not input_files:
            print(f"error: no {pattern} files found in directory: {args.input}", file=sys.stderr)
            return 1

        success = 0
        failed = 0
        for input_file in input_files:
            out_path = args.output / f"{input_file.stem}{suffix}"
            try:
                if mode == "xml_to_bin":
                    conv = Converter()
                    conv.convert_file(input_file, out_path)
                else:
                    deconv = Deconverter()
                    deconv.convert_file(input_file, out_path)
                success += 1
                print(f"Converted: {input_file} -> {out_path}")
            except Exception as exc:
                failed += 1
                print(f"error: conversion failed for {input_file}: {exc}", file=sys.stderr)

        print(f"Summary: {success} succeeded, {failed} failed")
        return 0 if failed == 0 else 1

    # Single file mode
    try:
        mode = _detect_conversion_mode(args.input, args.output)
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    try:
        if mode == "xml_to_bin":
            conv = Converter()
            conv.convert_file(args.input, args.output)
        else:
            deconv = Deconverter()
            deconv.convert_file(args.input, args.output)
    except Exception as exc:
        print(f"error: conversion failed: {exc}", file=sys.stderr)
        return 1

    print(f"Converted: {args.input} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
