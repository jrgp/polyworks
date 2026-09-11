#pragma once

#include "map_document.h"

#include <cstdint>

typedef unsigned int GLuint;

class TextureManager;

class Renderer {
public:
    void setTextureManager(TextureManager* mgr);
    void initialize();
    void renderAll(const MapDocument& doc, int viewW, int viewH, const ViewSettings& view);

    /* The Move tool draws a dashed bounding rectangle with corner/edge handles
       and the rotation-centre marker around the selection
       (frmOpenSoldatMapEditor.frm:3548-3612).  The viewport tells the renderer
       when that tool is the active one. */
    void setMoveToolActive(bool active) { m_moveToolActive = active; }

private:
    /* World-anchored background quad, sized from the map bounds like the
       original's mnuRefreshBG_Click (frm:14057). */
    void renderBackgroundQuad(const MapDocument& doc);
    /* backgroundPass=true draws only polygon types 24/25 (Background and
       Background Transition), which the original renders *before* back
       scenery; false draws every other type afterwards
       (frmOpenSoldatMapEditor.frm:2916-3062). */
    void renderPolygons(const MapDocument& doc, GLuint texId, bool backgroundPass);
    bool sceneryDrawSize(const MapDocument& doc, const EditorScenery& scenery,
                         unsigned int texId, float& width, float& height) const;
    void renderScenery(const MapDocument& doc, int level);
    void renderSelectionOverlays(const MapDocument& doc);
    void renderGrid(const MapDocument& doc, int viewW, int viewH);
    void renderSpawns(const MapDocument& doc);
    void renderWaypoints(const MapDocument& doc);
    void renderColliders(const MapDocument& doc);
    void renderLights(const MapDocument& doc);
    void renderSketchLines(const MapDocument& doc);
    void renderMoveSelectionRect(const MapDocument& doc);
    void renderGostek(const MapDocument& doc);
    /* Draws one 32x32 cell of the 8x4 objects.bmp atlas centred on a screen
       point (frmOpenSoldatMapEditor.frm:3200-3322).  Returns false when the
       atlas is unavailable, so callers can fall back to primitives. */
    bool drawObjectSprite(int col, int row, float cx, float cy, float sizePx,
                          uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    GLuint objectsTexture();

    TextureManager* m_texMgr = nullptr;
    bool m_initialized = false;
    bool m_moveToolActive = false;
    GLuint m_objectsTex = 0;
    bool   m_objectsTexTried = false;
};
