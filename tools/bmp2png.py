#!/usr/bin/env python3
"""把查看器导出的 24 位 BMP 转成 PNG（写实验报告时用）。

只依赖标准库：BMP 是未压缩的 24 位，PNG 用 zlib 手写三个 chunk 即可。
用法: python tools/bmp2png.py shot.bmp [shot.png]
"""
import struct
import sys
import zlib


def read_bmp(path):
    with open(path, "rb") as f:
        data = f.read()

    if data[:2] != b"BM":
        raise ValueError("not a BMP file")

    pix_off = struct.unpack_from("<I", data, 10)[0]
    hdr_size = struct.unpack_from("<I", data, 14)[0]
    if hdr_size < 40:
        raise ValueError("unsupported BMP header (need BITMAPINFOHEADER)")

    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24:
        raise ValueError("only 24-bit BMP supported, got %d" % bpp)

    bottom_up = h > 0
    h = abs(h)
    stride = (w * 3 + 3) & ~3

    # 统一转成自上而下的 RGB 行
    rows = []
    for y in range(h):
        src_y = (h - 1 - y) if bottom_up else y
        off = pix_off + src_y * stride
        row = bytearray(w * 3)
        for x in range(w):
            b, g, r = data[off + x * 3], data[off + x * 3 + 1], data[off + x * 3 + 2]
            row[x * 3], row[x * 3 + 1], row[x * 3 + 2] = r, g, b
        rows.append(bytes(row))
    return w, h, rows


def _chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def write_png(path, w, h, rows):
    raw = b"".join(b"\x00" + r for r in rows)          # 每行前置 filter 类型 0
    out = (b"\x89PNG\r\n\x1a\n"
           + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + _chunk(b"IDAT", zlib.compress(raw, 6))
           + _chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(out)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
    w, h, rows = read_bmp(src)
    write_png(dst, w, h, rows)
    print("%s (%dx%d) -> %s" % (src, w, h, dst))
    return 0


if __name__ == "__main__":
    sys.exit(main())
