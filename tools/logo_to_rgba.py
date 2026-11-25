#!/usr/bin/env python3
"""Convert an 8-bit RGBA, non-interlaced PNG to raw RGBA bytes (no PIL needed).

Regenerate the embedded logo (macOS, `sips` does the resize):
  sips --resampleWidth 640 assets/mads_logo.png --out /tmp/l.png
  python3 tools/logo_to_rgba.py /tmp/l.png assets/mads_logo.rgba
Then update kLogoW/kLogoH in src/ui/app_window.cpp.
"""
import struct, sys, zlib

data = open(sys.argv[1], "rb").read()
assert data[:8] == b"\x89PNG\r\n\x1a\n", "not a PNG"
pos, idat = 8, b""
while pos < len(data):
    n, kind = struct.unpack(">I4s", data[pos:pos + 8])
    body = data[pos + 8:pos + 8 + n]
    if kind == b"IHDR":
        w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        assert (depth, ctype, interlace) == (8, 6, 0), "need 8-bit RGBA, non-interlaced"
    elif kind == b"IDAT":
        idat += body
    pos += 12 + n
raw, bpp, stride = zlib.decompress(idat), 4, w * 4
out, prev = bytearray(), bytearray(stride)
for y in range(h):
    f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
    for i in range(stride):
        a = line[i - bpp] if i >= bpp else 0
        b, c = prev[i], prev[i - bpp] if i >= bpp else 0
        if f == 1: line[i] = (line[i] + a) & 255
        elif f == 2: line[i] = (line[i] + b) & 255
        elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
        elif f == 4:
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
    out += line
    prev = line
open(sys.argv[2], "wb").write(out)
print(f"{w}x{h} -> {len(out)} bytes")
