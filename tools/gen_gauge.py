#!/usr/bin/env python3
"""Build the squid fan-gauge asset.

Renders the Krake mark once in full colour, then once per part with every other
path blanked, to learn which pixel belongs to which tentacle. Emits per-row RLE
runs of (len, part, lit_colour, dim_colour) so the renderer can light tentacles
up to the fan level and grey out the rest.

Parts: 1..8 tentacles ordered left to right, 9 = head (mantle, eyes, smile).
"""
import re, struct, subprocess, sys, zlib

SVG, OUT, SIZE = sys.argv[1], sys.argv[2], int(sys.argv[3])
RSVG = ["nix", "shell", "nixpkgs#librsvg", "--command", "rsvg-convert"]
src = open(SVG).read()

path_d = [m.span(1) for m in re.finditer(r'<path[^>]*\sd="([^"]*)"', src)]
assert len(path_d) == 10, f"expected 8 tentacles + mantle + smile, got {len(path_d)}"
TENTACLES, MANTLE, SMILE = list(range(8)), 8, 9


def variant(keep):
    """SVG with only the listed path indices kept (others blanked)."""
    out, last = [], 0
    for i, (a, b) in enumerate(path_d):
        out.append(src[last:a])
        out.append(src[a:b] if i in keep else "")
        last = b
    out.append(src[last:])
    text = "".join(out)
    if MANTLE not in keep:  # eyes ride with the head
        text = text.replace('fill="#b48cf5"', 'fill="none"')
    return text


def render(text, name):
    p = f"/tmp/gauge_{name}.svg"
    open(p, "w").write(text)
    png = f"/tmp/gauge_{name}.png"
    subprocess.run(RSVG + ["-w", str(SIZE), "-h", str(SIZE), "-o", png, p],
                   check=True, capture_output=True)
    return decode(png)


def decode(path):
    raw = open(path, "rb").read()
    pos, idat = 8, bytearray()
    while pos < len(raw):
        ln, typ = struct.unpack(">I4s", raw[pos:pos + 8])
        body = raw[pos + 8:pos + 8 + ln]
        if typ == b"IHDR":
            w, h, depth, color, _, _, il = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and color == 6 and il == 0
        elif typ == b"IDAT":
            idat += body
        elif typ == b"IEND":
            break
        pos += 12 + ln
    data = zlib.decompress(bytes(idat))
    stride, prev, rows, off = w * 4, bytearray(w * 4), [], 0
    for _ in range(h):
        f = data[off]; off += 1
        line = bytearray(data[off:off + stride]); off += stride
        for i in range(stride):
            a = line[i - 4] if i >= 4 else 0
            b = prev[i]
            c = prev[i - 4] if i >= 4 else 0
            if f == 1:   line[i] = (line[i] + a) & 0xFF
            elif f == 2: line[i] = (line[i] + b) & 0xFF
            elif f == 3: line[i] = (line[i] + ((a + b) >> 1)) & 0xFF
            elif f == 4:
                p_ = a + b - c
                pa, pb, pc = abs(p_ - a), abs(p_ - b), abs(p_ - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        rows.append(line); prev = line
    return w, h, rows


full_w, full_h, full = render(variant(set(range(10))), "full")
head = render(variant({MANTLE, SMILE}), "head")[2]
tents = [render(variant({i}), f"t{i}")[2] for i in TENTACLES]

# Order tentacles left to right by their mean x, so the gauge fills predictably.
def mean_x(mask):
    tot = n = 0
    for y in range(full_h):
        for x in range(full_w):
            a = mask[y][x * 4 + 3]
            if a > 40:
                tot += x; n += 1
    return tot / max(n, 1)

order = sorted(range(8), key=lambda i: mean_x(tents[i]))
rank = {t: i + 1 for i, t in enumerate(order)}


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


offsets, blob = [], bytearray()
for y in range(full_h):
    runs, prev_key, count = [], None, 0
    for x in range(full_w):
        r, g, b, a = full[y][x * 4:x * 4 + 4]
        r, g, b = (r * a) // 255, (g * a) // 255, (b * a) // 255
        lit = rgb565(r, g, b)
        # Head wins where it overlaps a tentacle, matching the draw order.
        part = 0
        if head[y][x * 4 + 3] > 40:
            part = 9
        else:
            best, best_a = 0, 40
            for i in TENTACLES:
                av = tents[i][y][x * 4 + 3]
                if av > best_a:
                    best, best_a = rank[i], av
            part = best
        lum = (r * 30 + g * 59 + b * 11) // 100
        dim = rgb565(lum // 3, lum // 3, lum // 3)
        key = (part, lit, dim)
        if key == prev_key and count < 255:
            count += 1
        else:
            if prev_key is not None:
                runs.append((count, prev_key))
            prev_key, count = key, 1
    runs.append((count, prev_key))
    offsets.append(len(blob))
    blob.append(len(runs))
    for count, (part, lit, dim) in runs:
        blob += bytes([count, part, lit & 0xFF, lit >> 8, dim & 0xFF, dim >> 8])

with open(OUT, "w") as f:
    f.write('#pragma once\n\n#include "Particle.h"\n\n')
    f.write("// Generated from the tycstation Krake mark by tools/gen_gauge.py.\n")
    f.write("// Per-row RLE: [run_count] then run_count * [len, part, lit_lo,\n")
    f.write("// lit_hi, dim_lo, dim_hi]. Parts: 1-8 tentacles left to right,\n")
    f.write("// 9 = head, 0 = background. Never hand-edit.\n")
    f.write(f"const uint16_t kGaugeW = {full_w};\n")
    f.write(f"const uint16_t kGaugeH = {full_h};\n")
    f.write("const uint8_t kGaugeTentacles = 8;\n\n")
    f.write(f"const uint16_t kGaugeRows[{full_h}] = {{\n")
    for i in range(0, len(offsets), 12):
        f.write("    " + ", ".join(str(v) for v in offsets[i:i + 12]) + ",\n")
    f.write("};\n\n")
    f.write(f"const uint8_t kGaugeData[{len(blob)}] = {{\n")
    for i in range(0, len(blob), 16):
        f.write("    " + ", ".join(f"0x{v:02x}" for v in blob[i:i + 16]) + ",\n")
    f.write("};\n")

print(f"{full_w}x{full_h}: {len(blob)} bytes of runs + {full_h*2} offsets")
print("tentacle order (left to right):", order)
