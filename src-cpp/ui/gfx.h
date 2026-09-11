#pragma once
/*
 * gfx.h — loading the original's own bitmaps and cursors.
 *
 * PolyWorks does not draw its toolbar with text: frmTools blits 32x32 icons out
 * of skins/<skin>/tool_gfx.bmp, three columns wide (normal, hover, selected)
 * and one row per tool (frmTools.frm:395-425).  Reproducing the tool palette
 * therefore means loading that sheet, not picking lookalike glyphs, and the
 * same goes for the 27 .cur files the editor switches between as the effective
 * tool changes.
 *
 * .cur files are ICO-format images with a hotspot, and neither stb_image nor
 * GLFW reads them, so there is a small decoder here.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace pw {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;   /* width * height * 4 */
    bool valid() const { return width > 0 && height > 0; }
};

/* Load a PNG/BMP/JPG through stb_image.  `magentaIsTransparent` applies the
   original's colour key: its skin bitmaps mark transparent pixels with pure
   magenta rather than carrying an alpha channel. */
Image loadImage(const std::string& path, bool magentaIsTransparent = false);

/* Decode a Windows .cur.  Returns an empty image when the file is not a cursor
   or uses a format the original never shipped.  The hotspot is the one stored
   in the file, which is what makes a crosshair cursor point where it should. */
Image loadCursor(const std::string& path, int& hotspotX, int& hotspotY);

/* Upload an image as a GL texture.  Returns 0 on failure. */
unsigned int uploadTexture(const Image& img);

}  // namespace pw
