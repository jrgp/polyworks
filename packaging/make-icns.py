#!/usr/bin/env python3
"""Build a macOS .icns from the original PolyWorks Windows .ico.

    make-icns.py installer/PW.ico build/PW.icns

PolyWorks has always shipped one application icon, installer/PW.ico, and the
Mac bundle should show that icon rather than a newly drawn one or wxWidgets'
default.  The only obstacle is format, so convert rather than substitute.

Written against nothing but the standard library, and identical on macOS and
Linux, because the alternative -- sips(1) and iconutil(1) -- is macOS-only,
cannot be exercised on the machines where this is developed, and gives no
control over how a 48x48 icon is enlarged.

That last point matters.  PW.ico holds 16x16 and 48x48 frames; Finder wants up
to 512x512.  Smooth interpolation turns a hand-placed 48-pixel image into a
blurred smear, so enlargement here is nearest-neighbour: the result is chunky,
which is honest, instead of soft, which is not.
"""

import os
import struct
import sys
import zlib

# ---------------------------------------------------------------------------
# ICO reading.
#
# An .ico is an ICONDIR followed by ICONDIRENTRYs, each pointing at either an
# embedded PNG (Vista and later) or a "BMP" that is a BITMAPINFOHEADER, a
# palette, the colour rows and finally a 1-bit AND mask.  Both appear in the
# wild; PW.ico uses the latter.  Rows are bottom-up and padded to 4 bytes.
# ---------------------------------------------------------------------------


class Image:
    """A straightforward RGBA image; pixels[y][x] is an (r, g, b, a) tuple."""

    def __init__(self, width, height, pixels):
        self.width = width
        self.height = height
        self.pixels = pixels


def _read_bmp_frame(data, width, height):
    (hdr_size, _w, _h, _planes, bpp, compression) = struct.unpack_from("<IiiHHI", data, 0)
    if compression != 0:
        raise ValueError("compressed icon frames (BI_RLE/BI_PNG) are unsupported")

    # The stored height covers the colour rows and the AND mask together.
    palette_len = 0
    if bpp <= 8:
        # biClrUsed, or the full palette when zero.
        clr_used = struct.unpack_from("<I", data, 32)[0]
        palette_len = (clr_used or (1 << bpp)) * 4

    off = hdr_size
    palette = []
    for i in range(palette_len // 4):
        b, g, r, _ = data[off + i * 4: off + i * 4 + 4]
        palette.append((r, g, b))
    off += palette_len

    row_bytes = ((width * bpp + 31) // 32) * 4
    mask_bytes = ((width + 31) // 32) * 4

    rows = []
    for y in range(height):
        base = off + y * row_bytes
        row = []
        for x in range(width):
            if bpp == 32:
                b, g, r, a = data[base + x * 4: base + x * 4 + 4]
                row.append((r, g, b, a))
            elif bpp == 24:
                b, g, r = data[base + x * 3: base + x * 3 + 3]
                row.append((r, g, b, 255))
            elif bpp == 8:
                r, g, b = palette[data[base + x]]
                row.append((r, g, b, 255))
            elif bpp == 4:
                byte = data[base + x // 2]
                idx = (byte >> 4) if x % 2 == 0 else (byte & 0x0F)
                r, g, b = palette[idx]
                row.append((r, g, b, 255))
            elif bpp == 1:
                byte = data[base + x // 8]
                idx = (byte >> (7 - x % 8)) & 1
                r, g, b = palette[idx]
                row.append((r, g, b, 255))
            else:
                raise ValueError("unsupported icon colour depth: %d bpp" % bpp)
        rows.append(row)

    # Apply the AND mask, which is what makes the icon's background
    # transparent.  32-bit frames carry a real alpha channel and ignore it.
    mask_off = off + row_bytes * height
    if bpp != 32 and mask_off + mask_bytes * height <= len(data):
        for y in range(height):
            base = mask_off + y * mask_bytes
            for x in range(width):
                if (data[base + x // 8] >> (7 - x % 8)) & 1:
                    r, g, b, _ = rows[y][x]
                    rows[y][x] = (r, g, b, 0)

    rows.reverse()  # BMP rows are stored bottom-up.
    return Image(width, height, rows)


def _read_png_frame(data):
    """Decode the PNG subset that icon frames use (8-bit RGB/RGBA, no interlace)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG frame")
    pos, idat, width, height, bpp_ch = 8, b"", 0, 0, 0
    palette = []
    trns = b""
    color_type = 0
    while pos < len(data):
        length, ctype = struct.unpack_from(">I4s", data, pos)
        chunk = data[pos + 8: pos + 8 + length]
        if ctype == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or interlace != 0:
                raise ValueError("unsupported PNG icon frame")
            bpp_ch = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color_type]
        elif ctype == b"PLTE":
            palette = [tuple(chunk[i:i + 3]) for i in range(0, len(chunk), 3)]
        elif ctype == b"tRNS":
            trns = chunk
        elif ctype == b"IDAT":
            idat += chunk
        elif ctype == b"IEND":
            break
        pos += 12 + length

    raw = zlib.decompress(idat)
    stride = width * bpp_ch
    prev = bytearray(stride)
    rows = []
    p = 0
    for _ in range(height):
        filt = raw[p]; p += 1
        line = bytearray(raw[p:p + stride]); p += stride
        for i in range(stride):
            a = line[i - bpp_ch] if i >= bpp_ch else 0
            b = prev[i]
            c = prev[i - bpp_ch] if i >= bpp_ch else 0
            if filt == 1:
                line[i] = (line[i] + a) & 0xFF
            elif filt == 2:
                line[i] = (line[i] + b) & 0xFF
            elif filt == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif filt == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        prev = line
        row = []
        for x in range(width):
            px = line[x * bpp_ch: x * bpp_ch + bpp_ch]
            if color_type == 6:
                row.append(tuple(px))
            elif color_type == 2:
                row.append((px[0], px[1], px[2], 255))
            elif color_type == 4:
                row.append((px[0], px[0], px[0], px[1]))
            elif color_type == 3:
                r, g, b = palette[px[0]]
                a = trns[px[0]] if px[0] < len(trns) else 255
                row.append((r, g, b, a))
            else:
                row.append((px[0], px[0], px[0], 255))
        rows.append(row)
    return Image(width, height, rows)


def read_ico(path):
    with open(path, "rb") as fh:
        data = fh.read()
    reserved, kind, count = struct.unpack_from("<HHH", data, 0)
    if reserved != 0 or kind != 1 or count == 0:
        raise ValueError("%s is not a Windows .ico" % path)

    frames = []
    problems = []
    for i in range(count):
        w, h, _colors, _r, _planes, _bpp, size, offset = struct.unpack_from(
            "<BBBBHHII", data, 6 + i * 16)
        w = w or 256
        h = h or 256
        blob = data[offset:offset + size]
        try:
            frames.append(_read_png_frame(blob) if blob[:8] == b"\x89PNG\r\n\x1a\n"
                          else _read_bmp_frame(blob, w, h))
        except (ValueError, IndexError, KeyError, struct.error) as exc:
            # An .ico may mix encodings, and a frame this cannot read is only
            # fatal if it was the only one.  Skipping keeps an unusual frame
            # from costing the icon entirely.
            problems.append("%dx%d: %s" % (w, h, exc))

    if not frames:
        raise ValueError("no readable frames in %s (%s)"
                         % (path, "; ".join(problems) or "empty"))
    for p in problems:
        sys.stderr.write("make-icns: skipping frame %s\n" % p)
    return frames


# ---------------------------------------------------------------------------
# Scaling and PNG writing.
# ---------------------------------------------------------------------------

def resize_nearest(img, size):
    if img.width == size and img.height == size:
        return img
    rows = []
    for y in range(size):
        sy = min(img.height - 1, y * img.height // size)
        src = img.pixels[sy]
        rows.append([src[min(img.width - 1, x * img.width // size)] for x in range(size)])
    return Image(size, size, rows)


def write_png(img):
    raw = bytearray()
    for row in img.pixels:
        raw.append(0)  # filter: none
        for r, g, b, a in row:
            raw += bytes((r, g, b, a))

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload
                + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", img.width, img.height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
            + chunk(b"IEND", b""))


# ---------------------------------------------------------------------------
# ICNS writing.
#
# The container is 'icns', a big-endian total length, then typed entries.  The
# types below all take a PNG payload, which is what lets this avoid Apple's
# legacy packbits-compressed formats entirely.  icp4/icp5 cover the 16 and 32
# point sizes; ic07/ic08/ic09 cover 128, 256 and 512, which double as the
# Retina variants of 64, 128 and 256.
# ---------------------------------------------------------------------------

ICNS_TYPES = [(b"icp4", 16), (b"icp5", 32), (b"ic07", 128),
              (b"ic08", 256), (b"ic09", 512)]


def build_icns(frames):
    # Start from the largest frame available; for the small sizes prefer a
    # frame authored at exactly that size, since a hand-drawn 16x16 always
    # beats a downscaled 48x48.
    largest = max(frames, key=lambda f: f.width * f.height)
    exact = {f.width: f for f in frames if f.width == f.height}

    body = b""
    for tag, size in ICNS_TYPES:
        png = write_png(resize_nearest(exact.get(size, largest), size))
        body += tag + struct.pack(">I", len(png) + 8) + png
    return b"icns" + struct.pack(">I", len(body) + 8) + body


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: make-icns.py <input.ico> <output.icns>\n")
        return 2
    src, dst = argv[1], argv[2]

    try:
        frames = read_ico(src)
    except (OSError, ValueError, struct.error) as exc:
        sys.stderr.write("make-icns: %s\n" % exc)
        return 1
    icns = build_icns(frames)

    out_dir = os.path.dirname(os.path.abspath(dst))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(dst, "wb") as fh:
        fh.write(icns)

    sizes = ", ".join("%dx%d" % (f.width, f.height) for f in frames)
    sys.stderr.write("%s -> %s (from frames: %s)\n"
                     % (os.path.basename(src), os.path.basename(dst), sizes))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
