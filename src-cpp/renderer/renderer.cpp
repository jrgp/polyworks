#include "renderer.h"

#include "texture_manager.h"
#include "pms_types.h"
#include "geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

#if defined(__has_include)
#if __has_include(<GL/gl.h>)
#include <GL/gl.h>
#define PW_RENDERER_HAS_OPENGL 1
#elif __has_include(<OpenGL/gl.h>)
#include <OpenGL/gl.h>
#define PW_RENDERER_HAS_OPENGL 1
#else
#define PW_RENDERER_HAS_OPENGL 0
#endif
#else
#define PW_RENDERER_HAS_OPENGL 0
#endif

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadToDeg = 180.0f / kPi;

Vec2 toScreen(const MapDocument& doc, float x, float y) {
    return doc.worldToScreen({x, y});
}

#if PW_RENDERER_HAS_OPENGL
void setArgbColor(uint32_t color) {
    glColor4ub(argb_r(color), argb_g(color), argb_b(color), argb_a(color));
}

void drawScreenSquare(float x, float y, float halfSize) {
    glBegin(GL_QUADS);
    glVertex2f(x - halfSize, y - halfSize);
    glVertex2f(x + halfSize, y - halfSize);
    glVertex2f(x + halfSize, y + halfSize);
    glVertex2f(x - halfSize, y + halfSize);
    glEnd();
}

void drawCircle(float cx, float cy, float radius, int segments, GLenum mode) {
    glBegin(mode);
    for (int i = 0; i < segments; ++i) {
        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(segments);
        glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
    }
    glEnd();
}

void drawCrosshair(float cx, float cy, float radius) {
    glBegin(GL_LINES);
    glVertex2f(cx - radius, cy);
    glVertex2f(cx + radius, cy);
    glVertex2f(cx, cy - radius);
    glVertex2f(cx, cy + radius);
    glVertex2f(cx - radius * 0.7f, cy - radius * 0.7f);
    glVertex2f(cx + radius * 0.7f, cy + radius * 0.7f);
    glVertex2f(cx - radius * 0.7f, cy + radius * 0.7f);
    glVertex2f(cx + radius * 0.7f, cy - radius * 0.7f);
    glEnd();
}
#endif

std::array<uint8_t, 4> sceneryTint(const EditorScenery& scenery) {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = scenery.alpha;

    if (scenery.color != -1) {
        const uint32_t color = static_cast<uint32_t>(scenery.color);
        r = argb_r(color);
        g = argb_g(color);
        b = argb_b(color);
        a = static_cast<uint8_t>((static_cast<unsigned int>(a) * argb_a(color)) / 255U);
    }

    return {r, g, b, a};
}

}  // namespace

void Renderer::setTextureManager(TextureManager* mgr) {
    m_texMgr = mgr;
}

void Renderer::initialize() {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    if (m_texMgr != nullptr) {
        m_texMgr->getNotFoundTexture();
    }
#endif
    m_initialized = true;
}

void Renderer::renderAll(const MapDocument& doc, int viewW, int viewH, const ViewSettings& view) {
#if PW_RENDERER_HAS_OPENGL
    if (!m_initialized) {
        initialize();
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    if (view.showBackground) {
        renderBackgroundQuad(doc);
    }

    GLuint mapTexId = 0;
    if (view.showTexture && m_texMgr != nullptr &&
        !doc.options.textureName.empty()) {
        /* No placeholder: SetMapTexture (frm:4279) has no notfound fallback,
           and renderPolygons already draws vertex colours when there is no
           texture, which is what the original shows. */
        mapTexId = m_texMgr->loadTexture(doc.options.textureName, false);
    }

    /* Background polygons are drawn first so that back scenery sits on top of
       them, exactly as the original does (frm:2916). */
    if (view.showPolys) {
        renderPolygons(doc, mapTexId, true);
    }

    if (view.showScenery && view.showSceneryBack) {
        renderScenery(doc, SCENERY_BACK);
    }

    if (view.showPolys) {
        renderPolygons(doc, mapTexId, false);
    }

    if (view.showScenery && view.showSceneryMiddle) {
        renderScenery(doc, SCENERY_MIDDLE);
    }

    renderSelectionOverlays(doc);

    if (m_moveToolActive) {
        renderMoveSelectionRect(doc);
    }

    if (view.showScenery && view.showSceneryFront) {
        renderScenery(doc, SCENERY_FRONT);
    }

    if (view.showObjects) {
        renderSpawns(doc);
    }
    if (view.showWaypoints) {
        renderWaypoints(doc);
    }

    renderColliders(doc);

    if (view.showLights) {
        renderLights(doc);
    }
    if (view.showSketch) {
        renderSketchLines(doc);
    }
    renderGostek(doc);
    if (view.showGrid) {
        renderGrid(doc, viewW, viewH);
    }
#else
    (void)doc;
    (void)viewW;
    (void)viewH;
    (void)view;
#endif
}

void Renderer::renderBackgroundQuad(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    /* frm:14057 mnuRefreshBG_Click: the gradient quad is anchored in world
       space and spans the map bounds (measured against the origin) padded by
       640 units, so panning far outside the map reveals the clear colour. */
    float maxX = 0.0f, maxY = 0.0f, minX = 0.0f, minY = 0.0f;
    for (const auto& p : doc.polys) {
        for (const auto& v : p.v) {
            if (v.world.x > maxX) maxX = v.world.x;
            if (v.world.x < minX) minX = v.world.x;
            if (v.world.y > maxY) maxY = v.world.y;
            if (v.world.y < minY) minY = v.world.y;
        }
    }
    const float xOffset = std::floor((maxX + minX) * 0.5f);
    const float yOffset = std::floor((maxY + minY) * 0.5f);
    /* The original uses xOffset in both branches (frm:14084-14088). */
    const float bgSize = ((maxX - minX) > (maxY - minY)) ? (maxX - xOffset)
                                                         : (maxY - xOffset);
    const float ext = bgSize + 640.0f;

    const float left   = (xOffset - ext - doc.scrollX) * doc.zoom;
    const float right  = (xOffset + ext - doc.scrollX) * doc.zoom;
    const float top    = (yOffset - ext - doc.scrollY) * doc.zoom;
    const float bottom = (yOffset + ext - doc.scrollY) * doc.zoom;

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glBegin(GL_QUADS);
    setArgbColor(doc.options.bgColor1);
    glVertex2f(left, top);
    glVertex2f(right, top);
    setArgbColor(doc.options.bgColor2);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
#else
    (void)doc;
#endif
}

namespace {
/* Preferences store blend factors as indices into the original's combo list
   (frmPreferences.frx:0x172): ZERO, ONE, SRCCOLOR, INVSRCCOLOR, DESTCOLOR,
   INVDESTCOLOR, SRCALPHA, INVSRCALPHA. */
GLenum blendFactor(int index) {
#if PW_RENDERER_HAS_OPENGL
    switch (index) {
    case 0: return GL_ZERO;
    case 1: return GL_ONE;
    case 2: return GL_SRC_COLOR;
    case 3: return GL_ONE_MINUS_SRC_COLOR;
    case 4: return GL_DST_COLOR;
    case 5: return GL_ONE_MINUS_DST_COLOR;
    case 6: return GL_SRC_ALPHA;
    case 7: default: return GL_ONE_MINUS_SRC_ALPHA;
    }
#else
    (void)index;
    return 0;
#endif
}
}  // namespace

void Renderer::renderPolygons(const MapDocument& doc, GLuint texId,
                              bool backgroundPass) {
#if PW_RENDERER_HAS_OPENGL
    const bool textured = doc.viewSettings.showTexture && texId != 0;
    if (textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texId);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    glEnable(GL_BLEND);
    /* "Blend Polys" swaps in the user-configured factors (frm:2952-2955). */
    if (doc.viewSettings.blendPolys)
        glBlendFunc(blendFactor(doc.viewSettings.polyBlendSrc),
                    blendFactor(doc.viewSettings.polyBlendDest));
    else
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto inPass = [backgroundPass](const EditorPoly& p) {
        const bool isBackground = (p.polyType == 24 || p.polyType == 25);
        return isBackground == backgroundPass;
    };

    for (const auto& poly : doc.polys) {
        if (!inPass(poly)) continue;
        glBegin(GL_TRIANGLES);
        for (const auto& vertex : poly.v) {
            glColor4ub(vertex.r, vertex.g, vertex.b, vertex.alpha);
            if (textured) {
                glTexCoord2f(vertex.tu, vertex.tv);
            }
            const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
            glVertex2f(screen.x, screen.y);
        }
        glEnd();
    }

    if (doc.viewSettings.showWireframe) {
        glDisable(GL_TEXTURE_2D);
        if (doc.viewSettings.blendWireframe) {
            glEnable(GL_BLEND);
            glBlendFunc(blendFactor(doc.viewSettings.wireBlendSrc),
                        blendFactor(doc.viewSettings.wireBlendDest));
        } else {
            glDisable(GL_BLEND);
        }
        glColor4ub(32, 32, 32, 255);
        glLineWidth(1.0f);
        for (const auto& poly : doc.polys) {
            if (!inPass(poly)) continue;
            glBegin(GL_LINE_LOOP);
            for (const auto& vertex : poly.v) {
                const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
                glVertex2f(screen.x, screen.y);
            }
            glEnd();
        }
    }
#else
    (void)doc;
    (void)texId;
#endif
}

void Renderer::renderScenery(const MapDocument& doc, int level) {
#if PW_RENDERER_HAS_OPENGL
    if (m_texMgr == nullptr) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    for (const auto& scenery : doc.scenery) {
        if (scenery.level != level || scenery.style <= 0 ||
            scenery.style >= static_cast<int>(doc.sceneryNames.size())) {
            continue;
        }

        const std::string& textureName = doc.sceneryNames[scenery.style];
        const GLuint texId = m_texMgr->loadTexture(textureName);
        if (texId == 0) {
            continue;
        }

        int texW = 0;
        int texH = 0;
        m_texMgr->getSize(texId, texW, texH);
        const float width = static_cast<float>(scenery.width > 0 ? scenery.width : texW);
        const float height = static_cast<float>(scenery.height > 0 ? scenery.height : texH);
        if (width <= 0.0f || height <= 0.0f) {
            continue;
        }

        const auto tint = sceneryTint(scenery);
        const Vec2 screen = toScreen(doc, scenery.x, scenery.y);

        glBindTexture(GL_TEXTURE_2D, texId);
        glColor4ub(tint[0], tint[1], tint[2], tint[3]);
        glPushMatrix();
        /* VB6 draws the sprite with rotationCenter = (0,0) and translation =
           screenTr (frm:2879/2987), i.e. the map coordinate is the sprite's
           top-left corner and the sprite rotates about that corner.  The sign
           is negative because the outline math at frm:3421 maps local (w,0) to
           (+cos*w, -sin*w) in the y-down screen frame. */
        glTranslatef(screen.x, screen.y, 0.0f);
        glRotatef(-scenery.rotation * kRadToDeg, 0.0f, 0.0f, 1.0f);
        glScalef(scenery.scaleX * doc.zoom, scenery.scaleY * doc.zoom, 1.0f);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(width, 0.0f);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(width, height);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, height);
        glEnd();
        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);
#else
    (void)doc;
    (void)level;
#endif
}

void Renderer::renderSelectionOverlays(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    /* VB6 draws selected polygons as an additive fill tinted by
       gPolyTypeColors(polyType) (frmOpenSoldatMapEditor.frm:3082-3116), so the
       highlight colour tells the user the polygon's type at a glance.  Only
       *selected vertices* receive the tint; unselected ones stay black, which
       under additive blending contributes nothing.  Index 0 (Normal) is the
       user-configurable selection colour (modConfig.bas:79, default CE4D4A). */
    if (doc.viewSettings.showPolys) {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBegin(GL_TRIANGLES);
        for (const auto& poly : doc.polys) {
            if (!poly.anySelected()) {
                continue;
            }
            const uint32_t rgb = polyTypeColor(poly.polyType, doc.selectionColor);
            for (const auto& vertex : poly.v) {
                if (vertex.selected) {
                    glColor4ub(static_cast<GLubyte>((rgb >> 16) & 0xFF),
                               static_cast<GLubyte>((rgb >> 8) & 0xFF),
                               static_cast<GLubyte>(rgb & 0xFF), 255);
                } else {
                    glColor4ub(0, 0, 0, 255);
                }
                const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
                glVertex2f(screen.x, screen.y);
            }
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glLineWidth(2.0f);
    glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
    for (const auto& poly : doc.polys) {
        if (!poly.anySelected()) {
            continue;
        }
        glBegin(GL_LINE_LOOP);
        for (const auto& vertex : poly.v) {
            const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
            glVertex2f(screen.x, screen.y);
        }
        glEnd();
    }
    glLineWidth(1.0f);

    if (doc.viewSettings.showPoints) {
        glColor4ub(255, 255, 255, 255);
        for (const auto& poly : doc.polys) {
            for (const auto& vertex : poly.v) {
                const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
                drawScreenSquare(screen.x, screen.y, vertex.selected ? 3.0f : 2.0f);
            }
        }
    }

    for (const auto& poly : doc.polys) {
        for (const auto& vertex : poly.v) {
            if (!vertex.selected) {
                continue;
            }
            const Vec2 screen = toScreen(doc, vertex.world.x, vertex.world.y);
            glColor4ub(255, 255, 255, 255);
            drawScreenSquare(screen.x, screen.y, 2.0f);
            glColor4ub(255, 255, 0, 255);
            glBegin(GL_LINE_LOOP);
            glVertex2f(screen.x - 3.0f, screen.y - 3.0f);
            glVertex2f(screen.x + 3.0f, screen.y - 3.0f);
            glVertex2f(screen.x + 3.0f, screen.y + 3.0f);
            glVertex2f(screen.x - 3.0f, screen.y + 3.0f);
            glEnd();
        }
    }

    /* VB6 frm:3383 draws every scenery sprite's outline/corner points, gated on
       showScenery.  The outline appears when wireframe is on or the sprite is
       selected; the anchor point appears when showPoints is on or the sprite is
       selected, and the other three corners only when the SceneryVerts
       preference is enabled (frm:3436). */
    if (doc.viewSettings.showScenery) {
        for (const auto& scenery : doc.scenery) {
            float width = static_cast<float>(scenery.width);
            float height = static_cast<float>(scenery.height);
            if ((width <= 0.0f || height <= 0.0f) && m_texMgr != nullptr && scenery.style > 0 &&
                scenery.style < static_cast<int>(doc.sceneryNames.size())) {
                const GLuint texId = m_texMgr->loadTexture(doc.sceneryNames[scenery.style]);
                int texW = 0;
                int texH = 0;
                m_texMgr->getSize(texId, texW, texH);
                if (width <= 0.0f) {
                    width = static_cast<float>(texW);
                }
                if (height <= 0.0f) {
                    height = static_cast<float>(texH);
                }
            }
            if (width <= 0.0f || height <= 0.0f) {
                continue;
            }

            const bool drawOutline = scenery.selected || doc.viewSettings.showWireframe;
            const bool drawPoints  = scenery.selected || doc.viewSettings.showPoints;
            if (!drawOutline && !drawPoints) {
                continue;
            }

            /* frm:3421: corner 1 = anchor + (cos, -sin) * w, corner 3 = anchor +
               (sin, cos) * h, corner 2 = corner1 + corner3 - anchor. */
            const float w = width  * scenery.scaleX * doc.zoom;
            const float h = height * scenery.scaleY * doc.zoom;
            const float c = std::cos(scenery.rotation);
            const float s = std::sin(scenery.rotation);
            const Vec2  a = toScreen(doc, scenery.x, scenery.y);
            const Vec2  corner[4] = {
                a,
                { a.x + c * w,          a.y - s * w },
                { a.x + c * w + s * h,  a.y - s * w + c * h },
                { a.x + s * h,          a.y + c * h },
            };

            if (scenery.selected) {
                glColor4ub(255, 255, 0, 255);
            } else {
                glColor4ub(255, 255, 255, 160);
            }

            if (drawOutline) {
                glEnable(GL_LINE_STIPPLE);
                glLineStipple(1, 0x00FF);
                glBegin(GL_LINE_LOOP);
                for (const auto& cv : corner) glVertex2f(cv.x, cv.y);
                glEnd();
                glDisable(GL_LINE_STIPPLE);
            }

            if (drawPoints) {
                glPointSize(3.0f);
                glBegin(GL_POINTS);
                glVertex2f(corner[0].x, corner[0].y);
                if (doc.viewSettings.sceneryVerts) {
                    for (int i = 1; i < 4; ++i) glVertex2f(corner[i].x, corner[i].y);
                }
                glEnd();
            }
        }
    }
#else
    (void)doc;
#endif
}

void Renderer::renderMoveSelectionRect(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    if (!doc.selectionBounds(minX, minY, maxX, maxY)) return;

    const Vec2 tl = toScreen(doc, minX, minY);
    const Vec2 br = toScreen(doc, maxX, maxY);
    const Vec2 corner[4] = {tl, {br.x, tl.y}, br, {tl.x, br.y}};

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* VB6 textures the rectangle with lines.bmp, a 64px dash strip; a line
       stipple reproduces the same dashed appearance without the bitmap. */
    glColor4ub(255, 255, 255, 128);
    glLineWidth(1.0f);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(2, 0x00FF);
    glBegin(GL_LINE_LOOP);
    for (const auto& c : corner) glVertex2f(c.x, c.y);
    glEnd();
    glDisable(GL_LINE_STIPPLE);

    /* Corner handles and edge midpoint handles. */
    glPointSize(5.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 4; ++i) {
        glVertex2f(corner[i].x, corner[i].y);
        const Vec2& n = corner[(i + 1) % 4];
        glVertex2f((corner[i].x + n.x) * 0.5f, (corner[i].y + n.y) * 0.5f);
    }
    glEnd();

    /* The rotation/scale pivot marker is hidden in Fixed mode, matching
       `If Not mnuFixedRCenter.Checked` (frm:3606). */
    if (doc.rCenterMode != MapDocument::RCenterMode::Fixed) {
        Vec2 pivot;
        if (doc.rCenterMode == MapDocument::RCenterMode::Set)
            pivot = toScreen(doc, doc.rCenter.x, doc.rCenter.y);
        else
            pivot = {(tl.x + br.x) * 0.5f, (tl.y + br.y) * 0.5f};
        glColor4ub(255, 96, 96, 220);
        glPointSize(9.0f);
        glBegin(GL_POINTS);
        glVertex2f(pivot.x, pivot.y);
        glEnd();
    }

    glPointSize(1.0f);
    glDisable(GL_BLEND);
#else
    (void)doc;
#endif
}

void Renderer::renderGrid(const MapDocument& doc, int viewW, int viewH) {
#if PW_RENDERER_HAS_OPENGL
    const float gridStep = std::max(doc.viewSettings.gridSize, 1.0f);
    const int   divisions = std::max(doc.viewSettings.gridDivisions, 1);
    const float minorStep = gridStep / static_cast<float>(divisions);
    const float left = doc.scrollX;
    const float top = doc.scrollY;
    const float right = left + static_cast<float>(viewW) / doc.zoom;
    const float bottom = top + static_cast<float>(viewH) / doc.zoom;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Two passes: minor subdivision lines first, then the major lines on top,
       matching the original's colour1/colour2 + opacity1/opacity2 scheme
       (frm:5990-6007). */
    for (int pass = 0; pass < 2; ++pass) {
        const bool major = (pass == 1);
        if (!major && divisions < 2) continue;
        const unsigned col = major ? doc.viewSettings.gridColor1
                                   : doc.viewSettings.gridColor2;
        const float alpha = major ? doc.viewSettings.gridAlpha1
                                  : doc.viewSettings.gridAlpha2;
        glColor4ub(static_cast<GLubyte>((col >> 16) & 0xFF),
                   static_cast<GLubyte>((col >> 8) & 0xFF),
                   static_cast<GLubyte>(col & 0xFF),
                   static_cast<GLubyte>(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f));
        const float step = major ? gridStep : minorStep;
        const float startX = std::floor(left / step) * step;
        const float startY = std::floor(top / step) * step;
        glBegin(GL_LINES);
        for (float worldX = startX; worldX <= right; worldX += step) {
            /* Skip positions already covered by the major pass. */
            if (!major && std::fabs(worldX / gridStep - std::round(worldX / gridStep)) < 1e-4f)
                continue;
            const float screenX = (worldX - doc.scrollX) * doc.zoom;
            glVertex2f(screenX, 0.0f);
            glVertex2f(screenX, static_cast<float>(viewH));
        }
        for (float worldY = startY; worldY <= bottom; worldY += step) {
            if (!major && std::fabs(worldY / gridStep - std::round(worldY / gridStep)) < 1e-4f)
                continue;
            const float screenY = (worldY - doc.scrollY) * doc.zoom;
            glVertex2f(0.0f, screenY);
            glVertex2f(static_cast<float>(viewW), screenY);
        }
        glEnd();
    }
    glDisable(GL_BLEND);
#else
    (void)doc;
    (void)viewW;
    (void)viewH;
#endif
}

GLuint Renderer::objectsTexture() {
#if PW_RENDERER_HAS_OPENGL
    if (!m_objectsTexTried) {
        m_objectsTexTried = true;
        if (m_texMgr != nullptr) m_objectsTex = m_texMgr->loadTexture("objects.bmp");
        /* loadTexture falls back to the notfound placeholder; treat that as
           "no atlas" so the primitive fallbacks stay in charge. */
        if (m_texMgr != nullptr && m_objectsTex == m_texMgr->getNotFoundTexture())
            m_objectsTex = 0;
    }
    return m_objectsTex;
#else
    return 0;
#endif
}

bool Renderer::drawObjectSprite(int col, int row, float cx, float cy,
                                float sizePx, uint8_t r, uint8_t g, uint8_t b,
                                uint8_t a) {
#if PW_RENDERER_HAS_OPENGL
    const GLuint tex = objectsTexture();
    if (tex == 0) return false;

    /* objects.bmp is an 8-column, 4-row atlas of 32x32 cells (frm:3213). */
    const float du = 1.0f / 8.0f;
    const float dv = 1.0f / 4.0f;
    const float u0 = col * du, v0 = row * dv;
    const float h  = sizePx * 0.5f;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(u0,      v0);      glVertex2f(cx - h, cy - h);
    glTexCoord2f(u0 + du, v0);      glVertex2f(cx + h, cy - h);
    glTexCoord2f(u0 + du, v0 + dv); glVertex2f(cx + h, cy + h);
    glTexCoord2f(u0,      v0 + dv); glVertex2f(cx - h, cy + h);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    return true;
#else
    (void)col; (void)row; (void)cx; (void)cy; (void)sizePx;
    (void)r; (void)g; (void)b; (void)a;
    return false;
#endif
}

void Renderer::renderGostek(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    if (!doc.showGostek) return;
    /* The gostek is a 32x32 scale reference drawn from atlas cell (2,2)
       tinted grey (frm:3310-3322). */
    const Vec2 s = toScreen(doc, doc.gostek.x, doc.gostek.y);
    const float size = 32.0f * doc.zoom;
    if (!drawObjectSprite(2, 2, s.x, s.y, size, 128, 128, 128, 255)) {
        glDisable(GL_TEXTURE_2D);
        glColor4ub(128, 128, 128, 255);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(s.x - size * 0.5f, s.y - size * 0.5f);
        glVertex2f(s.x + size * 0.5f, s.y - size * 0.5f);
        glVertex2f(s.x + size * 0.5f, s.y + size * 0.5f);
        glVertex2f(s.x - size * 0.5f, s.y + size * 0.5f);
        glEnd();
    }
#else
    (void)doc;
#endif
}

void Renderer::renderSpawns(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    for (const auto& spawn : doc.spawns) {
        if (!spawn.active) {
            continue;
        }

        switch (spawn.team) {
        case SPAWN_ALPHA: glColor4ub(64, 128, 255, 255); break;
        case SPAWN_BRAVO: glColor4ub(255, 64, 64, 255); break;
        case SPAWN_CHARLIE: glColor4ub(255, 255, 0, 255); break;
        case SPAWN_DELTA: glColor4ub(0, 220, 0, 255); break;
        case SPAWN_FROGGER: glColor4ub(0, 255, 255, 255); break;
        case SPAWN_YELLOW: glColor4ub(255, 255, 0, 255); break;
        case SPAWN_RED: glColor4ub(255, 0, 0, 255); break;
        case SPAWN_GENERAL:
        default: glColor4ub(160, 160, 160, 255); break;
        }

        const Vec2 screen = toScreen(doc, spawn.x, spawn.y);
        /* VB6 draws each spawn from atlas cell (team mod 8, team div 8)
           at a fixed 32px, unaffected by zoom (frm:3225-3241). */
        const int cell = spawn.team;
        const bool drawn = drawObjectSprite(cell % 8, cell / 8, screen.x,
                                            screen.y, 32.0f, 255, 255, 255,
                                            255);
        if (!drawn) {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
            drawCircle(screen.x, screen.y, 5.0f, 16, GL_LINE_LOOP);
        }
    }
#else
    (void)doc;
#endif
}

void Renderer::renderWaypoints(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    std::unordered_map<int, Vec2> waypointPositions;
    for (const auto& waypoint : doc.waypoints) {
        if (waypoint.active) {
            waypointPositions.emplace(waypoint.id, toScreen(doc, waypoint.x, waypoint.y));
        }
    }

    glColor4ub(128, 128, 128, 255);
    glBegin(GL_LINES);
    for (const auto& waypoint : doc.waypoints) {
        if (!waypoint.active) {
            continue;
        }
        const auto fromIt = waypointPositions.find(waypoint.id);
        if (fromIt == waypointPositions.end()) {
            continue;
        }
        for (int connection : waypoint.connections) {
            const auto toIt = waypointPositions.find(connection);
            if (toIt == waypointPositions.end()) {
                continue;
            }
            glVertex2f(fromIt->second.x, fromIt->second.y);
            glVertex2f(toIt->second.x, toIt->second.y);
        }
    }
    glEnd();

    glColor4ub(160, 160, 160, 255);
    for (const auto& waypoint : doc.waypoints) {
        if (!waypoint.active) {
            continue;
        }
        const Vec2 screen = toScreen(doc, waypoint.x, waypoint.y);
        drawScreenSquare(screen.x, screen.y, 4.0f);
    }
#else
    (void)doc;
#endif
}

void Renderer::renderColliders(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor4ub(0, 255, 0, 255);
    for (const auto& collider : doc.colliders) {
        if (!collider.active) {
            continue;
        }
        const Vec2 screen = toScreen(doc, collider.x, collider.y);
        /* Colliders use atlas cell (1,2) scaled to their radius
           (frm:3288-3296). */
        if (!drawObjectSprite(1, 2, screen.x, screen.y,
                              collider.radius * 2.0f * doc.zoom,
                              255, 255, 255, 255)) {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
            glColor4ub(0, 255, 0, 255);
            drawCircle(screen.x, screen.y, collider.radius * doc.zoom, 16,
                       GL_LINE_LOOP);
        }
    }
#else
    (void)doc;
#endif
}

void Renderer::renderLights(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 0, 255);
    for (const auto& light : doc.lights) {
        const Vec2 screen = toScreen(doc, light.x, light.y);
        /* Lights use atlas cell (7,2) tinted with the light's own colour
           (frm:3255-3269). */
        if (!drawObjectSprite(7, 2, screen.x, screen.y, 32.0f,
                              light.r, light.g, light.b, 255)) {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_BLEND);
            glColor4ub(255, 255, 0, 255);
            drawCrosshair(screen.x, screen.y, 6.0f);
        }
    }
#else
    (void)doc;
#endif
}

void Renderer::renderSketchLines(const MapDocument& doc) {
#if PW_RENDERER_HAS_OPENGL
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glColor4ub(210, 210, 210, 255);
    glBegin(GL_LINES);
    for (const auto& line : doc.sketch) {
        const Vec2 a = toScreen(doc, line.a.x, line.a.y);
        const Vec2 b = toScreen(doc, line.b.x, line.b.y);
        glVertex2f(a.x, a.y);
        glVertex2f(b.x, b.y);
    }
    glEnd();
#else
    (void)doc;
#endif
}
