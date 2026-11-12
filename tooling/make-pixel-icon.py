"""Generate client/Platform/Windows/app.ico from the 16x16 pixel-art headphone (rafaelholder fork).

Black headphone with a light outline so it stays visible on dark taskbars. Each size is an integer
nearest-neighbour scale of the sprite, centred, so pixels stay crisp. Pure standard library: PNG
entries are written by hand (zlib) and packed into an ICO (PNG-in-ICO, Windows Vista+).

Keep SPRITE in sync with kHeadphoneSprite in client/Tray.cpp.

    python -I tooling/make-pixel-icon.py            # writes client/Platform/Windows/app.ico
    python -I tooling/make-pixel-icon.py --png out  # also writes out/icon-<size>.png previews
"""
import argparse
import struct
import zlib
from pathlib import Path

SPRITE = [
    "................",
    ".....######.....",
    "...##......##...",
    "..#..........#..",
    ".#............#.",
    ".#............#.",
    ".#............#.",
    "###..........###",
    "####........####",
    "####........####",
    "####........####",
    "####........####",
    ".###........###.",
    "................",
    "................",
    "................",
]
SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
FILL = (0, 0, 0, 255)
OUTLINE = (235, 235, 235, 255)
CLEAR = (0, 0, 0, 0)
ROOT = Path(__file__).resolve().parent.parent


def render(size: int) -> list[list[tuple[int, int, int, int]]]:
    n = len(SPRITE)
    scale = max(1, size // n)
    offset = (size - n * scale) // 2
    filled = [[False] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            sx, sy = (x - offset) // scale, (y - offset) // scale
            if 0 <= sx < n and 0 <= sy < n and SPRITE[sy][sx] == "#":
                filled[y][x] = True
    width = max(1, scale // 2)  # Outline thickness grows with the icon.
    pixels = [[CLEAR] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            if filled[y][x]:
                pixels[y][x] = FILL
                continue
            for dy in range(-width, width + 1):
                for dx in range(-width, width + 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < size and 0 <= xx < size and filled[yy][xx]:
                        pixels[y][x] = OUTLINE
                        break
                else:
                    continue
                break
    return pixels


def png(pixels: list[list[tuple[int, int, int, int]]]) -> bytes:
    height, width = len(pixels), len(pixels[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def ico(images: dict[int, bytes]) -> bytes:
    count = len(images)
    out = struct.pack("<HHH", 0, 1, count)
    offset = 6 + 16 * count
    entries, blobs = b"", b""
    for size, data in images.items():
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    return out + entries + blobs


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=ROOT / "client" / "Platform" / "Windows" / "app.ico")
    parser.add_argument("--png", type=Path, help="Folder for PNG previews of every size")
    args = parser.parse_args()
    images = {size: png(render(size)) for size in SIZES}
    args.out.write_bytes(ico(images))
    print(f"{args.out} ({len(images)} sizes)")
    if args.png:
        args.png.mkdir(parents=True, exist_ok=True)
        for size, data in images.items():
            (args.png / f"icon-{size}.png").write_bytes(data)
        print(f"previews in {args.png}")


if __name__ == "__main__":
    main()
