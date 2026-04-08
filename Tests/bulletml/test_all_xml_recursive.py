#!/usr/bin/env python3
"""Validate that all BulletML XML files under noiz2sa_share are test-covered.

For each XML file found recursively, this test:
1. Converts XML -> BLB using tools/bulletml_converter.py logic.
2. Converts BLB -> XML to validate round-trip decoding.

The test fails if any conversion raises an exception.
"""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]
NOIZ2SA_SHARE = PROJECT_ROOT / "noiz2sa_share"
TOOLS_DIR = PROJECT_ROOT / "tools"

sys.path.insert(0, str(TOOLS_DIR))
from bulletml_converter import Converter, Deconverter  # type: ignore  # noqa: E402


def find_xml_files(root: Path) -> list[Path]:
    return sorted(
        p
        for p in root.rglob("*")
        if p.is_file() and p.suffix.lower() == ".xml"
    )


def main() -> int:
    if not NOIZ2SA_SHARE.exists():
        print(f"error: missing directory: {NOIZ2SA_SHARE}", file=sys.stderr)
        return 1

    xml_files = find_xml_files(NOIZ2SA_SHARE)
    if not xml_files:
        print(f"error: no XML files found under {NOIZ2SA_SHARE}", file=sys.stderr)
        return 1

    failed: list[tuple[Path, str]] = []

    with tempfile.TemporaryDirectory(prefix="bulletml_xml_cov_") as tmp:
        tmp_dir = Path(tmp)

        for xml_path in xml_files:
            rel = xml_path.relative_to(NOIZ2SA_SHARE)
            out_blb = tmp_dir / rel.with_suffix(".blb")
            roundtrip_xml = tmp_dir / rel.with_suffix(".roundtrip.xml")

            try:
                Converter().convert_file(xml_path, out_blb)
                Deconverter().convert_file(out_blb, roundtrip_xml)
                print(f"PASS {rel}")
            except Exception as exc:
                failed.append((rel, str(exc)))
                print(f"FAIL {rel}: {exc}", file=sys.stderr)

    if failed:
        print(
            f"error: {len(failed)} of {len(xml_files)} XML files failed conversion",
            file=sys.stderr,
        )
        return 1

    print(f"All XML files covered: {len(xml_files)} / {len(xml_files)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
