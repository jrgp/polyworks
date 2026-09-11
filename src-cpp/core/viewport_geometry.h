#pragma once
/*
 * viewport_geometry.h — the screen -> viewport -> world coordinate pipeline.
 *
 * Lives in the core, with no GUI dependency, because getting this wrong is not
 * a cosmetic problem: the wxWidgets implementation shipped two bugs that both
 * came from conflating the coordinate spaces below, and both were invisible to
 * every test in the suite.  Keeping the arithmetic here means it can be tested
 * headlessly.
 *
 * There are four spaces, and the distinction between the first two is the one
 * that matters:
 *
 *   window pixels (framebuffer)
 *       What OpenGL draws into.  On a Retina display, or on Windows at 200%
 *       scaling, there are more of these than there are logical units.
 *
 *   window logical units (points)
 *       What GLFW reports cursor positions in, and what the UI is laid out in.
 *       framebufferScale = framebuffer pixels / logical unit.
 *
 *   viewport-local logical units
 *       The same units, but with the origin at the top-left corner of the map
 *       viewport rather than of the window.  The viewport is not the whole
 *       window: the menu bar is above it and the status bar below.  This is
 *       the space MapDocument calls "screen", and the space the renderer's
 *       glOrtho is set up in.
 *
 *   world units
 *       Map coordinates.  screen = (world - scroll) * zoom.
 *
 * The invariant that makes zoom-correct picking work is that *one* rectangle
 * describes the viewport, and both the GL projection and the mouse mapping are
 * derived from it -- never computed independently from the window size.
 */

#include "pms_types.h"

struct ViewportGeometry {
    /* Top-left of the map viewport, in logical units within the window. */
    float originX = 0.0f;
    float originY = 0.0f;
    /* Size of the map viewport, in logical units. */
    float width  = 0.0f;
    float height = 0.0f;
    /* Framebuffer pixels per logical unit.  1.0 on a non-scaled display, 2.0
       on a Retina screen, 1.25/1.5/2.0 at the corresponding Windows scalings. */
    float framebufferScale = 1.0f;

    bool valid() const { return width > 0.0f && height > 0.0f; }

    /* Viewport rectangle in framebuffer pixels, ready for glViewport/glScissor.
       `framebufferHeight` is the height of the whole window's framebuffer:
       OpenGL's origin is the bottom-left corner, the UI's is the top-left, so
       the y axis has to be flipped here and nowhere else.

       This is the fix for the "map only fills part of the window" bug: the
       viewport is sized in framebuffer pixels while the projection below is
       sized in logical units, so a Retina window no longer draws a
       half-size scene into a full-size rectangle. */
    void glRect(int framebufferHeight, int& x, int& y, int& w, int& h) const {
        x = static_cast<int>(originX * framebufferScale + 0.5f);
        w = static_cast<int>(width  * framebufferScale + 0.5f);
        h = static_cast<int>(height * framebufferScale + 0.5f);
        const int top = static_cast<int>(originY * framebufferScale + 0.5f);
        y = framebufferHeight - top - h;
    }

    /* True when a point given in logical window coordinates is over the map. */
    bool contains(float windowX, float windowY) const {
        return windowX >= originX && windowX < originX + width &&
               windowY >= originY && windowY < originY + height;
    }

    /* Logical window coordinates -> viewport-local ("screen") coordinates. */
    Vec2 windowToViewport(Vec2 window) const {
        return {window.x - originX, window.y - originY};
    }

    Vec2 viewportToWindow(Vec2 viewport) const {
        return {viewport.x + originX, viewport.y + originY};
    }
};

/* Normalise a mouse-wheel / trackpad scroll delta into whole zoom notches.
 *
 * A wheel click reports 1.0 on every platform GLFW supports.  A high-precision
 * trackpad reports a continuous stream of small fractions instead -- dozens of
 * events per gesture -- and treating each one as a notch is what made macOS
 * zoom uncontrollable: a single two-finger flick applied the 1.25x step
 * thirty times over, a factor of ~800.
 *
 * The accumulator is therefore carried across events and only whole notches
 * are consumed, so a gesture of a given physical length produces the same zoom
 * change whether it arrives as one event or fifty.  Direction reversal clears
 * the accumulator so that a flick back does not first have to undo leftover
 * travel.
 */
inline int consumeWheelNotches(float& accumulator, float delta) {
    if ((accumulator > 0.0f && delta < 0.0f) ||
        (accumulator < 0.0f && delta > 0.0f)) {
        accumulator = 0.0f;
    }
    accumulator += delta;

    int notches = 0;
    while (accumulator >= 1.0f) {
        accumulator -= 1.0f;
        ++notches;
    }
    while (accumulator <= -1.0f) {
        accumulator += 1.0f;
        --notches;
    }
    return notches;
}
