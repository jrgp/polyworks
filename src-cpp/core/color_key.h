#pragma once
/*
 * color_key.h — VB6's texture colour key.
 *
 * Every texture the original loads goes through D3DX CreateTextureFromFileEx
 * with COLOR_KEY = &HFF00FF00 (modGlobals.bas:12) — opaque pure green becomes
 * fully transparent.  Soldat's scenery and texture BMPs carry no alpha channel
 * and rely entirely on this convention, so skipping it draws every scenery
 * sprite inside a green box.
 *
 * Lives in core (rather than the renderer) so it is covered by the headless
 * test suite; it is pure pixel arithmetic with no OpenGL dependency.
 */

/* In-place over a tightly packed RGBA8 buffer of w*h texels. */
void applyColorKey(unsigned char* rgba, int w, int h);
