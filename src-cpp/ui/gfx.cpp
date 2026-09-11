#include "gfx.h"

#include "stb_image.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

/* Windows ships the OpenGL 1.1 headers and nothing newer, so a constant that
   has been core since 1.2 still has to be spelled out.  The value is fixed by
   the specification; every driver from that era onwards accepts it. */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace pw {
namespace {

uint16_t readU16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                std::istreambuf_iterator<char>());
}

}  // namespace

Image loadImage(const std::string& path, bool magentaIsTransparent) {
    Image out;
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (pixels == nullptr) {
        return out;
    }
    out.width = w;
    out.height = h;
    out.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
    stbi_image_free(pixels);

    if (magentaIsTransparent) {
        for (size_t i = 0; i + 3 < out.rgba.size(); i += 4) {
            if (out.rgba[i] == 255 && out.rgba[i + 1] == 0 &&
                out.rgba[i + 2] == 255) {
                out.rgba[i + 3] = 0;
            }
        }
    }
    return out;
}

/*
 * A .cur is an ICONDIR (6 bytes) plus one ICONDIRENTRY (16 bytes) per image;
 * for cursors the two "planes/bitcount" fields hold the hotspot instead.  Each
 * image is a BITMAPINFOHEADER whose biHeight is twice the real height, holding
 * the colour (XOR) bitmap above the 1-bit transparency (AND) mask.
 *
 * The shipped cursors are all 32x32 monochrome, but 4/8/24/32-bit entries are
 * handled too so that a custom skin is not silently ignored.
 */
Image loadCursor(const std::string& path, int& hotspotX, int& hotspotY) {
    hotspotX = 0;
    hotspotY = 0;
    Image out;

    const std::vector<uint8_t> data = readFile(path);
    if (data.size() < 22 || readU16(&data[0]) != 0 || readU16(&data[2]) != 2) {
        return out;   /* not a cursor */
    }
    const int count = readU16(&data[4]);
    if (count < 1) {
        return out;
    }

    const uint8_t* entry = &data[6];
    const int width  = entry[0] != 0 ? entry[0] : 256;
    const int height = entry[1] != 0 ? entry[1] : 256;
    hotspotX = readU16(entry + 4);
    hotspotY = readU16(entry + 6);
    const uint32_t size   = readU32(entry + 8);
    const uint32_t offset = readU32(entry + 12);
    if (offset + size > data.size() || size < 40) {
        return out;
    }

    const uint8_t* dib = &data[offset];
    const uint32_t headerSize = readU32(dib);
    if (headerSize < 40) {
        return out;
    }
    const int bits = readU16(dib + 14);
    const uint32_t compression = readU32(dib + 16);
    if (compression != 0) {
        return out;   /* BI_RGB only; the originals are all uncompressed */
    }

    uint32_t paletteEntries = readU32(dib + 32);
    if (paletteEntries == 0 && bits <= 8) {
        paletteEntries = 1u << bits;
    }
    const uint8_t* palette = dib + headerSize;
    const uint8_t* xorBits = palette + paletteEntries * 4;

    /* Rows are padded to a 4-byte boundary and stored bottom-up. */
    const size_t xorStride = ((static_cast<size_t>(width) * bits + 31) / 32) * 4;
    const size_t andStride = ((static_cast<size_t>(width) + 31) / 32) * 4;
    const uint8_t* andBits = xorBits + xorStride * static_cast<size_t>(height);
    if (andBits + andStride * static_cast<size_t>(height) > dib + size) {
        return out;
    }

    out.width = width;
    out.height = height;
    out.rgba.assign(static_cast<size_t>(width) * height * 4, 0);

    for (int y = 0; y < height; ++y) {
        const size_t srcRow = static_cast<size_t>(height - 1 - y);
        const uint8_t* xorRow = xorBits + srcRow * xorStride;
        const uint8_t* andRow = andBits + srcRow * andStride;
        for (int x = 0; x < width; ++x) {
            uint8_t r = 0, g = 0, b = 0, a = 255;
            const bool masked =
                (andRow[x >> 3] & (0x80 >> (x & 7))) != 0;

            switch (bits) {
            case 1: {
                const int idx = (xorRow[x >> 3] & (0x80 >> (x & 7))) ? 1 : 0;
                b = palette[idx * 4 + 0];
                g = palette[idx * 4 + 1];
                r = palette[idx * 4 + 2];
                break;
            }
            case 4: {
                const int idx = (x & 1) ? (xorRow[x >> 1] & 0x0F)
                                        : (xorRow[x >> 1] >> 4);
                b = palette[idx * 4 + 0];
                g = palette[idx * 4 + 1];
                r = palette[idx * 4 + 2];
                break;
            }
            case 8: {
                const int idx = xorRow[x];
                b = palette[idx * 4 + 0];
                g = palette[idx * 4 + 1];
                r = palette[idx * 4 + 2];
                break;
            }
            case 24:
                b = xorRow[x * 3 + 0];
                g = xorRow[x * 3 + 1];
                r = xorRow[x * 3 + 2];
                break;
            case 32:
                b = xorRow[x * 4 + 0];
                g = xorRow[x * 4 + 1];
                r = xorRow[x * 4 + 2];
                a = xorRow[x * 4 + 3];
                break;
            default:
                return Image{};
            }

            /* The AND mask wins for everything below 32 bits: mask set means
               "leave the screen alone", which is transparent.  (A set mask with
               a white XOR pixel means "invert the screen"; no windowing system
               supports that any more, so it is drawn as opaque white, which is
               what Windows itself does for cursors on a composited desktop.) */
            if (bits < 32 && masked) {
                a = (r | g | b) ? 255 : 0;
            }

            uint8_t* px = &out.rgba[(static_cast<size_t>(y) * width + x) * 4];
            px[0] = r;
            px[1] = g;
            px[2] = b;
            px[3] = a;
        }
    }
    return out;
}

unsigned int uploadTexture(const Image& img) {
    if (!img.valid()) {
        return 0;
    }
    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) {
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, img.rgba.data());
    return id;
}

}  // namespace pw
