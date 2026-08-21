#!/usr/bin/env python3
"""
Rasterize / convert the Leon Editor Windows icon.

Writes BMP/DIB .ico (not PNG-in-ICO) plus a 256px PNG for glfwSetWindowIcon.
No third-party deps — stdlib zlib PNG decode/encode only.

Usage:
  python Scripts/make-editor-icon.py
  python Scripts/make-editor-icon.py --src path/to/source.png
"""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOGO_PNG = ROOT / "Engine" / "Resources" / "Icon" / "Logo.png"
ICON_DIR = ROOT / "Editor" / "Resources" / "Icons"
OUT_PNG = ICON_DIR / "LeonEditor.png"
OUT_ICO = ICON_DIR / "LeonEditor.ico"
ICO_SIZES = (16, 24, 32, 48, 64, 256)


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png(data: bytes) -> tuple[int, int, bytearray]:
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")

    width = height = bit_depth = color_type = interlace = 0
    idat = bytearray()
    pos = 8
    while pos + 12 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        tag = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            width, height, bit_depth, color_type, _comp, _filt, interlace = struct.unpack(
                ">IIBBBBB", chunk
            )
        elif tag == b"IDAT":
            idat.extend(chunk)
        elif tag == b"IEND":
            break

    if width <= 0 or height <= 0 or bit_depth != 8 or interlace != 0:
        raise ValueError(f"unsupported PNG ({width}x{height} depth={bit_depth} type={color_type})")
    if color_type not in (0, 2, 4, 6):
        raise ValueError(f"unsupported PNG color type {color_type}")

    bpp = {0: 1, 2: 3, 4: 2, 6: 4}[color_type]
    raw = zlib.decompress(bytes(idat))
    stride = width * bpp
    rows: list[bytearray] = []
    cursor = 0
    prev = bytearray(stride)
    for _ in range(height):
        ftype = raw[cursor]
        scan = bytearray(raw[cursor + 1 : cursor + 1 + stride])
        cursor += 1 + stride
        if ftype == 1:
            for i in range(stride):
                left = scan[i - bpp] if i >= bpp else 0
                scan[i] = (scan[i] + left) & 255
        elif ftype == 2:
            for i in range(stride):
                scan[i] = (scan[i] + prev[i]) & 255
        elif ftype == 3:
            for i in range(stride):
                left = scan[i - bpp] if i >= bpp else 0
                scan[i] = (scan[i] + ((left + prev[i]) // 2)) & 255
        elif ftype == 4:
            for i in range(stride):
                left = scan[i - bpp] if i >= bpp else 0
                up = prev[i]
                ul = prev[i - bpp] if i >= bpp else 0
                scan[i] = (scan[i] + paeth(left, up, ul)) & 255
        elif ftype != 0:
            raise ValueError(f"unsupported PNG filter {ftype}")
        rows.append(scan)
        prev = scan

    rgba = bytearray(width * height * 4)
    for y, row in enumerate(rows):
        for x in range(width):
            o = (y * width + x) * 4
            p = x * bpp
            if color_type == 0:
                rgba[o : o + 4] = bytes((row[p], row[p], row[p], 255))
            elif color_type == 2:
                rgba[o : o + 4] = bytes((row[p], row[p + 1], row[p + 2], 255))
            elif color_type == 4:
                rgba[o : o + 4] = bytes((row[p], row[p], row[p], row[p + 1]))
            else:
                rgba[o : o + 4] = row[p : p + 4]
    return width, height, rgba


def encode_png(width: int, height: int, rgba: bytes | bytearray) -> bytes:
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba[y * stride : (y + 1) * stride])

    def chunk(tag: bytes, payload: bytes) -> bytes:
        crc = zlib.crc32(tag)
        crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )


def resize_rgba(src: bytes | bytearray, sw: int, sh: int, dw: int, dh: int) -> bytearray:
    dst = bytearray(dw * dh * 4)
    for y in range(dh):
        y0 = y * sh // dh
        y1 = max(y0 + 1, (y + 1) * sh // dh)
        for x in range(dw):
            x0 = x * sw // dw
            x1 = max(x0 + 1, (x + 1) * sw // dw)
            r = g = b = a = n = 0
            for sy in range(y0, y1):
                row = sy * sw * 4
                for sx in range(x0, x1):
                    p = row + sx * 4
                    r += src[p]
                    g += src[p + 1]
                    b += src[p + 2]
                    a += src[p + 3]
                    n += 1
            o = (y * dw + x) * 4
            dst[o] = r // n
            dst[o + 1] = g // n
            dst[o + 2] = b // n
            dst[o + 3] = a // n
    return dst


def bmp_dib_icon(rgba: bytes | bytearray, size: int) -> bytes:
    # 32-bit DIB: BITMAPINFOHEADER + BGRA XOR (bottom-up) + 1-bit AND mask.
    row_xor = size * 4
    and_stride = ((size + 31) // 32) * 4
    xor = bytearray(row_xor * size)
    for y in range(size):
        src_y = size - 1 - y
        dst = y * row_xor
        src = src_y * row_xor
        for x in range(size):
            s = src + x * 4
            d = dst + x * 4
            xor[d] = rgba[s + 2]
            xor[d + 1] = rgba[s + 1]
            xor[d + 2] = rgba[s]
            xor[d + 3] = rgba[s + 3]
    header = struct.pack(
        "<IIIHHIIIIII",
        40,
        size,
        size * 2,
        1,
        32,
        0,
        len(xor),
        0,
        0,
        0,
        0,
    )
    return header + bytes(xor) + (b"\x00" * (and_stride * size))


def write_ico(path: Path, images: list[tuple[int, bytes]]) -> None:
    count = len(images)
    offset = 6 + 16 * count
    entries = bytearray()
    blobs = bytearray()
    for size, blob in images:
        w = 0 if size >= 256 else size
        entries.extend(
            struct.pack("<BBBBHHII", w, w, 0, 0, 1, 32, len(blob), offset)
        )
        blobs.extend(blob)
        offset += len(blob)
    path.write_bytes(b"\x00\x00\x01\x00" + struct.pack("<H", count) + bytes(entries) + bytes(blobs))


def main() -> int:
    parser = argparse.ArgumentParser(description="Build LeonEditor.ico from Engine/Resources/Icon/Logo.png")
    parser.add_argument("--src", type=Path, default=None, help="Source PNG (defaults to Engine/Resources/Icon/Logo.png)")
    args = parser.parse_args()

    src = args.src if args.src is not None else LOGO_PNG
    if not src.is_file():
        raise SystemExit(f"No source PNG at {src}")

    width, height, rgba = decode_png(src.read_bytes())
    print(f"[INFO] Source {src} ({width}x{height})")

    ICON_DIR.mkdir(parents=True, exist_ok=True)
    png256 = rgba if width == 256 and height == 256 else resize_rgba(rgba, width, height, 256, 256)
    OUT_PNG.write_bytes(encode_png(256, 256, png256))
    print(f"[INFO] Wrote {OUT_PNG}")

    images: list[tuple[int, bytes]] = []
    for size in ICO_SIZES:
        pixels = rgba if width == size and height == size else resize_rgba(rgba, width, height, size, size)
        images.append((size, bmp_dib_icon(pixels, size)))
    write_ico(OUT_ICO, images)
    print(f"[INFO] Wrote {OUT_ICO} sizes={list(ICO_SIZES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
