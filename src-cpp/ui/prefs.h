#pragma once
/*
 * prefs.h — the settings polyworks.ini holds.
 *
 * Lifted unchanged from the wxWidgets dialog header it used to live in; the
 * values, their defaults and the modConfig.bas references behind them are the
 * original's, and none of that changes with the GUI toolkit.
 */

#include <string>

namespace pw {

struct AppPrefs {
    /* Zoom */
    float   minZoom      = 0.0625f;
    float   maxZoom      = 16.0f;
    float   resetZoom    = 1.0f;

    /* Grid */
    int     gridSpacing  = 32;
    int     gridDivisions = 4;
    unsigned int gridColor1 = 0xFF000000;
    unsigned int gridColor2 = 0xFF000000;
    float   gridAlpha1   = 1.0f;
    float   gridAlpha2   = 0.2f;

    /* Snap */
    bool    snapEnabled  = false;
    float   snapRadius   = 8.0f;

    /* Undo */
    int     undoDepth    = 16;

    /* frmPreferences picScenery, captioned "Use 4 verts for scenery"
       (modConfig.bas:82 SceneryVerts, default False). */
    bool    sceneryVerts = false;

    /* Paths */
    std::string soldatDir;
    std::string prefabsDir;
    std::string uncompDir;

    /* Colors */
    unsigned int pointColor     = 0xFFFFFFFF;
    unsigned int selectionColor = 0xFFFFFF00;

    /* Blending (frmPreferences cboPolySrc / cboPolyDest / cboWireSrc /
       cboWireDest).  Indices into the original's blend-factor list. */
    int polyBlendSrc  = 6;
    int polyBlendDest = 7;
    int wireBlendSrc  = 6;
    int wireBlendDest = 7;

    /* Palette tool settings.  modConfig.bas keeps these in [ToolSettings]
       so the painting colour, brush radius, opacity, blend mode and colour
       mode survive a restart (modConfig.bas:146-150, 330-334). */
    unsigned int paintColor = 0xFFFFFF;   /* CurrentColor, default FFFFFF */
    int     colorRadius     = 16;         /* ColorRadius, default 16 */
    float   colorOpacity    = 1.0f;       /* Opacity, stored as percent */
    int     colorBlendMode  = 0;          /* BlendMode */
    int     colorMode       = 1;          /* ColorMode, default Normal */
};

}  // namespace pw
