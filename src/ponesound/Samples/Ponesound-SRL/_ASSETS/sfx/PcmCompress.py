import os
from pathlib import Path
import json

# convert string to 4 chars
def fourcc(s: str) -> int:
    """
    Convert a 4-char string to big-endian uint32 FOURCC.
    Example: 'LZSS' or '.PCM'
    """
    if len(s) != 4:
        raise ValueError(f"FOURCC must be 4 chars: '{s}'")
    return int.from_bytes(s.encode("ascii"), "big")

# header for general use
def build_lzss_header(file_path: Path, original_size: int) -> bytes:
    MAGIC = fourcc("LZSS")
    VERSION = fourcc("v1.0")
    RESERVED = 0xFFFFFFFF

    # extension → FOURCC (".pcm" → ".PCM")
    ext = file_path.suffix.upper()
    if len(ext) > 4:
        ext = ext[:4]  # safety clamp
    ext = ext.ljust(4, "\x00")  # pad to 4 chars if needed

    FILETYPE = fourcc(ext)

    header = bytearray()
    header += MAGIC.to_bytes(4, "big")
    header += VERSION.to_bytes(4, "big")
    header += FILETYPE.to_bytes(4, "big")
    header += original_size.to_bytes(4, "big")
    header += RESERVED.to_bytes(4, "big")

    return bytes(header)

# header for .snd format / PCMs
def build_sound_header(bit_depth: int, sample_rate: int, original_size: int, compressed_size: int) -> bytes:
    header = bytearray()
    
    header += bit_depth.to_bytes(2, "big")
    header += sample_rate.to_bytes(2, "big")
    header += original_size.to_bytes(4, "big")
    header += compressed_size.to_bytes(4, "big")

    return bytes(header)

# LZSS compression
WINDOW_SIZE = 4096
LOOKAHEAD = 18
MIN_MATCH = 3

def lzss_compress(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    length = len(data)

    while i < length:
        flags_pos = len(out)
        out.append(0)
        flags = 0
        bit_count = 0

        for _ in range(8):
            if i >= length:
                break

            best_offset = 0
            best_length = 0

            start = max(0, i - WINDOW_SIZE)

            for j in range(start, i):
                k = 0
                while (k < LOOKAHEAD and
                       i + k < length and
                       data[j + k] == data[i + k]):
                    k += 1

                if k >= MIN_MATCH and k > best_length:
                    best_length = k
                    best_offset = i - j

            if best_length >= MIN_MATCH:
                # match = 0 bit
                flags <<= 1

                distance = best_offset
                length_field = best_length - MIN_MATCH

                encoded_offset = distance - 1

                pair = ((encoded_offset & 0xFFF) << 4) | (length_field & 0xF)

                out.append((pair >> 8) & 0xFF)
                out.append(pair & 0xFF)

                i += best_length
            else:
                # literal = 1 bit
                flags = (flags << 1) | 1
                out.append(data[i])
                i += 1

            bit_count += 1

        flags <<= (8 - bit_count)
        out[flags_pos] = flags & 0xFF

    return bytes(out)

# for reference / verification of output
def lzss_decompress(data: bytes, decompressed_size: int) -> bytes:
    inp = 0
    out = bytearray()

    flags = 0
    mask = 0

    while len(out) < decompressed_size:
        if mask == 0:
            flags = data[inp]
            inp += 1
            mask = 0x80  # MSB first

        if flags & mask:
            # literal
            out.append(data[inp])
            inp += 1
        else:
            # match
            pair = (data[inp] << 8) | data[inp + 1]
            inp += 2

            encoded_offset = (pair >> 4) & 0x0FFF
            length = (pair & 0x000F) + 3

            distance = encoded_offset + 1
            src = len(out) - distance
            if src < 0:
                raise ValueError(f"Invalid offset {offset} at output pos {len(out)}")

            for i in range(length):
                out.append(out[src + i])

        mask >>= 1

    return bytes(out)

# general file compression (specify extension)
def compressLzssInFolder(folder: str, write_output: bool = False):
    folder_path = Path(folder)

    print(f"\nLZSS test for folder: {folder_path}\n")

    for file_path in folder_path.glob("*.bin"):
        data = file_path.read_bytes()
        compressed = lzss_compress(data)

        original_size = len(data)
        compressed_size = len(compressed)

        if original_size > 0:
            ratio = compressed_size / original_size
            savings = 100 * (1 - ratio)
        else:
            ratio = 0
            savings = 0

        print(f"{file_path.name:20} "
              f"{original_size:8} -> {compressed_size:8} "
              f"ratio={ratio:.3f}  savings={savings:+.2f}%")

        if write_output:
            header = build_lzss_header(file_path, original_size)
            out_data = header + compressed

            out_path = file_path.with_suffix(".lz")
            out_path.write_bytes(out_data)

# specifically for packing .pcm samples to .snd format
def packSndInFolder(assets_folder: str, out_folder: str):
    assets_folder = Path(assets_folder)

    with open(assets_folder / "SOUND.json", "r") as f:
        config = json.load(f)

    assets_path = Path(assets_folder)
    out_path = Path(out_folder)

    for snd_name, files in config.items():
        print(f"\nBuilding {snd_name}")

        snd_data = bytearray()
                
        for pcm_name, info in files.items():
            pcm_path = assets_path / pcm_name
            data = pcm_path.read_bytes()

            bit_depth = int(info["BitDepth"])
            sample_rate = int(info["SampleRate"])

            compressed = lzss_compress(data)
            original_size=len(data)
            compressed_size = 0
            
            # only use compression if it helps
            if len(compressed) < len(data):
                payload = compressed
                compressed_size=len(payload)
            else:
                payload = data

            header = build_sound_header(
                bit_depth,
                sample_rate,
                compressed_size,
                original_size
            )

            snd_data += header + payload

            print(f"  {pcm_name:12} {len(data):6} -> {len(payload):6}")

        out_file = out_path / snd_name
        out_file.write_bytes(snd_data)
        print(f"Saved: {out_file}")

# process PCM files and save them to the project as .snd:
PROJECT_ROOT = Path(__file__).resolve().parent.parent
INPUT = PROJECT_ROOT / "sfx"
OUTPUT = PROJECT_ROOT.parent / "cd" / "data"

if __name__ == "__main__":
     packSndInFolder(INPUT, OUTPUT)
     
