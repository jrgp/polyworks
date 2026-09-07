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

private:
    void renderBackground(uint32_t col1, uint32_t col2, int w, int h);
    void renderPolygons(const MapDocument& doc, GLuint texId);
    void renderScenery(const MapDocument& doc, int level);
    void renderSelectionOverlays(const MapDocument& doc);
    void renderGrid(const MapDocument& doc, int viewW, int viewH);
    void renderSpawns(const MapDocument& doc);
    void renderWaypoints(const MapDocument& doc);
    void renderColliders(const MapDocument& doc);
    void renderLights(const MapDocument& doc);
    void renderSketchLines(const MapDocument& doc);

    TextureManager* m_texMgr = nullptr;
    bool m_initialized = false;
};
