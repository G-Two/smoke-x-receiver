#!/usr/bin/env python3
"""Render a 128x64 OLED preview PNG using the firmware font and sample text."""

from __future__ import annotations

import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONT_H = ROOT / "main" / "app_display_font.h"
DEFAULT_OUT = ROOT / "docs" / "heltec-oled-status.png"

WIDTH = 128
HEIGHT = 64
FONT_WIDTH = 6
FONT_HEIGHT = 8
SCALE = 4

SAMPLE_LINES = [
    "Smoke X Rx",
    "Up 00:15:32",
    "WiFi AP 192.168.4.1",
    "MQTT disabled",
    "LoRa sync 920 MHz",
    "ID unpaired",
]


def load_font(path: Path) -> bytes:
    text = path.read_text()
    match = re.search(r"font5x7\[475\] = \{(.*?)\};", text, re.S)
    if not match:
        raise SystemExit(f"Could not parse font from {path}")
    return bytes(int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", match.group(1)))


def draw_char(pixels: bytearray, font: bytes, x: int, y: int, char: str) -> None:
    code = ord(char)
    if code < 0x20 or code > 0x7E:
        code = ord("?")

    glyph = font[(code - 0x20) * 5 : (code - 0x20) * 5 + 5]
    for col, column in enumerate(glyph):
        for row in range(7):
            if (column >> row) & 0x01:
                px = x + col
                py = y + row
                if 0 <= px < WIDTH and 0 <= py < HEIGHT:
                    pixels[py * WIDTH + px] = 1


def draw_text(pixels: bytearray, font: bytes, x: int, y: int, text: str) -> None:
    cursor = x
    for char in text:
        draw_char(pixels, font, cursor, y, char)
        cursor += FONT_WIDTH
        if cursor > WIDTH - FONT_WIDTH:
            break


def render_lines(lines: list[str], font: bytes) -> bytearray:
    pixels = bytearray(WIDTH * HEIGHT)
    y = 0
    for line in lines:
        draw_text(pixels, font, 0, y, line)
        y += FONT_HEIGHT
    return pixels


def write_png(path: Path, pixels: bytearray) -> None:
    scaled_w = WIDTH * SCALE
    scaled_h = HEIGHT * SCALE

    rows = []
    for sy in range(scaled_h):
        src_y = sy // SCALE
        row = bytearray()
        for sx in range(scaled_w):
            src_x = sx // SCALE
            on = pixels[src_y * WIDTH + src_x]
            row.extend((0xFF, 0xFF, 0xFF) if on else (0x00, 0x00, 0x00))
        rows.append(b"\x00" + row)

    compressed = zlib.compress(b"".join(rows), 9)
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open("wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n")
        handle.write(
            struct.pack(">I", 13)
            + b"IHDR"
            + struct.pack(">IIBBBBB", scaled_w, scaled_h, 8, 2, 0, 0, 0)
        )
        handle.write(struct.pack(">I", zlib.crc32(b"IHDR" + struct.pack(">IIBBBBB", scaled_w, scaled_h, 8, 2, 0, 0, 0)) & 0xFFFFFFFF))
        handle.write(struct.pack(">I", len(compressed)) + b"IDAT" + compressed)
        handle.write(struct.pack(">I", zlib.crc32(b"IDAT" + compressed) & 0xFFFFFFFF))
        handle.write(struct.pack(">I", 0) + b"IEND")
        handle.write(struct.pack(">I", zlib.crc32(b"IEND") & 0xFFFFFFFF))


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    parser.add_argument("line", nargs="*", help="Optional custom lines")
    args = parser.parse_args()

    font = load_font(FONT_H)
    lines = args.line or SAMPLE_LINES
    pixels = render_lines(lines, font)
    write_png(args.output, pixels)
    print(f"Wrote {args.output} ({WIDTH}x{HEIGHT}, scaled {SCALE}x)")


if __name__ == "__main__":
    main()
