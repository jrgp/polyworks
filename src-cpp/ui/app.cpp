#include "app.h"

#include "gfx.h"
#include "platform.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl2.h"

#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace pw {
namespace {

/* The original's viewport background: picMap's BackColor, 0x0E1A2B in BGR
   notation, i.e. the dark brown 43,26,14. */
constexpr float kBackR = 43.0f / 255.0f;
constexpr float kBackG = 26.0f / 255.0f;
constexpr float kBackB = 14.0f / 255.0f;

App* g_app = nullptr;

void glfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

/* GLFW delivers file drops; the original accepts a map dropped onto its
   window (frm:1063 OLEDragDrop). */
void dropCallback(GLFWwindow*, int count, const char** paths) {
    if (g_app == nullptr || count < 1) {
        return;
    }
    std::string path = paths[0];
    Editor& ed = g_app->editor();
    ed.confirmDiscardChanges([&ed, path]() { ed.loadMap(path); });
}

}  // namespace

/* ---- lifecycle --------------------------------------------------------- */

bool App::initialise(int argc, char** argv) {
    g_app = this;

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "PolyWorks: unable to initialise GLFW.\n");
        return false;
    }

    /* A legacy (compatibility) context is required, not a core profile: both
       the map renderer and Dear ImGui's GL2 backend draw with the fixed
       function pipeline, which is the most direct expression of the original's
       Direct3D 7 immediate-mode drawing.  Asking for a forward-compatible core
       context here would leave macOS without glBegin. */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#endif
    /* Windows and Wayland report a per-monitor scale; letting GLFW size the
       window in logical units keeps the UI the same physical size at 100% and
       200% alike.  macOS already works this way. */
#if defined(GLFW_SCALE_TO_MONITOR)
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    /* The original opens maximised (frm:14431 sets WindowState); a fixed size
       large enough for the tool windows down both edges is the equivalent when
       the monitor is unknown, clamped to what actually fits. */
    int winW = 1280, winH = 860;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        if (mw > 0 && mh > 0) {
            winW = std::min(winW, mw);
            winH = std::min(winH, mh);
        }
    }
    m_window = glfwCreateWindow(winW, winH, "PolyWorks", nullptr, nullptr);
    if (m_window == nullptr) {
        std::fprintf(stderr,
                     "PolyWorks: unable to create an OpenGL window.\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    glfwSetDropCallback(m_window, dropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   /* window layout is part of the workspace file */
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL2_Init();

    m_editor.initialise(skinsPath());
    m_skin = loadSkinColors(m_editor.skinsPath);
    updateScale();

    loadSkinGraphics();
    loadCursors();

    m_editor.renderer.initialize();

    if (argc > 1 && argv[1] != nullptr && argv[1][0] != '\0') {
        m_editor.openCommandLineMap(argv[1]);
    }
    return true;
}

void App::shutdown() {
    m_editor.shutdown();

    for (GLFWcursor*& c : m_cursors) {
        if (c != nullptr) {
            glfwDestroyCursor(c);
            c = nullptr;
        }
    }
    if (m_panCursor != nullptr) {
        glfwDestroyCursor(m_panCursor);
        m_panCursor = nullptr;
    }

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    g_app = nullptr;
}

void App::requestQuit() {
    m_quitRequested = true;
}

void App::run() {
    std::string lastTitle;

    while (!m_quitRequested) {
        glfwPollEvents();

        /* The close button is a request, not an order: an unsaved map raises
           the same save prompt the File > Exit menu item does (frm:1010). */
        if (glfwWindowShouldClose(m_window) != 0) {
            glfwSetWindowShouldClose(m_window, GLFW_FALSE);
            m_editor.confirmDiscardChanges([this]() { requestQuit(); });
        }

        const std::string title = m_editor.windowTitle();
        if (title != lastTitle) {
            glfwSetWindowTitle(m_window, title.c_str());
            lastTitle = title;
        }

        buildFrame();
    }
}

/* ---- scaling ------------------------------------------------------------ */

void App::updateScale() {
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(m_window, &winW, &winH);
    glfwGetFramebufferSize(m_window, &fbW, &fbH);

    m_fbScale = (winW > 0) ? static_cast<float>(fbW) / static_cast<float>(winW)
                           : 1.0f;
    if (m_fbScale <= 0.0f) {
        m_fbScale = 1.0f;
    }

    float contentX = 1.0f, contentY = 1.0f;
    glfwGetWindowContentScale(m_window, &contentX, &contentY);

    /*
     * Two platforms, two conventions, one number.
     *
     * On Windows the window is measured in pixels, so a 150%-scaled display
     * reports contentScale 1.5 and framebufferScale 1.0: the UI has to be
     * drawn 1.5x larger or it comes out physically tiny -- which is exactly
     * the bug the wxWidgets build shipped.
     *
     * On macOS the window is measured in points, which are already
     * DPI-independent: contentScale 2.0 and framebufferScale 2.0 on a Retina
     * screen.  Scaling the UI by 2 there would make everything huge.
     *
     * contentScale / framebufferScale is 1.5 in the first case and 1.0 in the
     * second, which is the factor wanted in both.
     */
    const float wanted = std::max(0.5f, contentX / m_fbScale);
    if (std::fabs(wanted - m_appliedUiScale) > 0.01f) {
        m_uiScale = wanted;
        m_appliedUiScale = wanted;
        applyTheme(m_skin, m_uiScale);
        /* Glyphs are rasterised at framebuffer resolution and then drawn at
           logical size, so text stays sharp on a Retina panel instead of being
           a magnified 1x bitmap. */
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();
        ImFontConfig cfg;
        cfg.SizePixels = 13.0f * m_uiScale * m_fbScale;
        io.Fonts->AddFontDefault(&cfg);
        io.Fonts->Build();
        io.FontGlobalScale = 1.0f / m_fbScale;
        ImGui_ImplOpenGL2_DestroyFontsTexture();
    }
}

/* ---- frame -------------------------------------------------------------- */

void App::buildFrame() {
    updateScale();

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    drawMenuBar(*this);

    const float menuH = ImGui::GetFrameHeight();
    const float statusH = ImGui::GetFrameHeight();

    /* One rectangle, derived once, used by both the projection and the mouse
       mapping.  See viewport_geometry.h. */
    m_editor.viewport.originX = 0.0f;
    m_editor.viewport.originY = menuH;
    m_editor.viewport.width  = io.DisplaySize.x;
    m_editor.viewport.height = std::max(1.0f, io.DisplaySize.y - menuH - statusH);
    m_editor.viewport.framebufferScale = m_fbScale;

    if (m_editor.viewResetPending && m_editor.viewport.valid()) {
        m_editor.viewResetPending = false;
        m_editor.resetViewForLoadedMap();
    }

    drawStatusBar(statusH);
    drawPanels(*this);
    drawDialogs(*this);

    handleShortcuts();
    handleViewportInput();
    drawOpenContextMenu(*this);
    updateCursor();

    ImGui::Render();

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(kBackR, kBackG, kBackB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawViewport();

    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

void App::drawViewport() {
    const ViewportGeometry& vp = m_editor.viewport;
    if (!vp.valid()) {
        return;
    }

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);

    int x = 0, y = 0, w = 0, h = 0;
    vp.glRect(fbH, x, y, w, h);
    if (w <= 0 || h <= 0) {
        return;
    }

    /* The GL2 ImGui backend saves and restores the whole state block around
       its own drawing, but the map renderer does not, so its state is fenced
       off here rather than leaking into the UI pass. */
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glViewport(x, y, w, h);
    glScissor(x, y, w, h);
    glEnable(GL_SCISSOR_TEST);

    /* The viewport is sized in framebuffer pixels but the projection is in
       logical units: everything downstream -- picking tolerances, the 8-pixel
       vertex radius, line widths, the grid -- is expressed in the same units
       the mouse arrives in, and stays correct on a Retina display. */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, vp.width, vp.height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    m_editor.renderer.setMoveToolActive(
        m_editor.interaction.currentFunction() == TOOL_MOVE);
    m_editor.renderer.renderAll(m_editor.doc, static_cast<int>(vp.width),
                                static_cast<int>(vp.height),
                                m_editor.doc.viewSettings);

    /* Overlays that belong to an in-progress gesture rather than to the
       document: the vertices of a polygon being placed, the rubber band, and
       the straight-line sketch preview. */
    glDisable(GL_TEXTURE_2D);

    Vec2 verts[3];
    const int pending = m_editor.interaction.pendingVertices(verts);
    if (pending > 0) {
        glColor4ub(0, 220, 255, 255);
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < pending; ++i) {
            const Vec2 s = m_editor.doc.worldToScreen(verts[i]);
            glVertex2f(s.x, s.y);
        }
        glEnd();
        glPointSize(1.0f);
        if (pending > 1) {
            glBegin(GL_LINE_STRIP);
            for (int i = 0; i < pending; ++i) {
                const Vec2 s = m_editor.doc.worldToScreen(verts[i]);
                glVertex2f(s.x, s.y);
            }
            glEnd();
        }
    }

    Vec2 a{}, b{};
    if (m_editor.interaction.rubberBand(a, b)) {
        const Vec2 sa = m_editor.doc.worldToScreen(a);
        const Vec2 sb = m_editor.doc.worldToScreen(b);
        glEnable(GL_LINE_STIPPLE);
        glLineStipple(1, 0xF0F0);
        glColor4ub(255, 255, 0, 255);
        glBegin(GL_LINE_LOOP);
        glVertex2f(sa.x, sa.y);
        glVertex2f(sb.x, sa.y);
        glVertex2f(sb.x, sb.y);
        glVertex2f(sa.x, sb.y);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
    }

    if (m_editor.interaction.sketchPreview(a, b)) {
        const Vec2 sa = m_editor.doc.worldToScreen(a);
        const Vec2 sb = m_editor.doc.worldToScreen(b);
        glColor4ub(m_editor.palette.r, m_editor.palette.g, m_editor.palette.b,
                   255);
        glBegin(GL_LINES);
        glVertex2f(sa.x, sa.y);
        glVertex2f(sb.x, sb.y);
        glEnd();
    }

    glDisable(GL_SCISSOR_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

/* ---- input -------------------------------------------------------------- */

void App::handleViewportInput() {
    ImGuiIO& io = ImGui::GetIO();
    Interaction& ix = m_editor.interaction;

    Modifiers mods;
    mods.shift = io.KeyShift;
    mods.ctrl  = io.KeyCtrl || io.KeySuper;   /* Cmd stands in for Ctrl on macOS */
    mods.alt   = io.KeyAlt;
    mods.space = !io.WantCaptureKeyboard && ImGui::IsKeyDown(ImGuiKey_Space);
    ix.setModifiers(mods);

    const ViewportGeometry& vp = m_editor.viewport;
    const Vec2 window{io.MousePos.x, io.MousePos.y};
    const Vec2 local = vp.windowToViewport(window);
    const bool overViewport =
        vp.contains(window.x, window.y) && !io.WantCaptureMouse;

    /* A gesture that began in the viewport keeps receiving motion even when
       the cursor wanders over a floating panel, exactly as a captured mouse
       does; otherwise dragging a vertex past the Tools window would drop it. */
    const bool gestureActive = ix.state() != InteractionState::Idle;

    if (overViewport) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ix.onLeftDown(local);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            ix.onMiddleDown(local);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ix.onRightDown(local);
            drawViewportContextMenu(*this, m_editor.doc.screenToWorld(local));
        }
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            const int notches = consumeWheelNotches(m_wheelAccum, wheel);
            if (notches != 0) {
                ix.onWheel(notches, local);
            }
        }
    }

    if (overViewport || gestureActive) {
        ix.onMouseMove(local);
        m_editor.lastMouseWorld = m_editor.doc.screenToWorld(local);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ix.onLeftUp(local);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
        ix.onMiddleUp(local);
    }
}

void App::updateCursor() {
    ImGuiIO& io = ImGui::GetIO();
    const ViewportGeometry& vp = m_editor.viewport;
    if (io.WantCaptureMouse || !vp.contains(io.MousePos.x, io.MousePos.y)) {
        return;   /* leave ImGui's own cursor choice alone */
    }

    GLFWcursor* cursor = nullptr;
    if (m_editor.interaction.state() == InteractionState::Panning) {
        cursor = m_panCursor;
    } else {
        const int fn = m_editor.interaction.currentFunction();
        if (fn >= 0 && fn < TOOL_COUNT) {
            cursor = m_cursors[static_cast<size_t>(fn)];
        }
    }
    /* Setting the cursor after ImGui's backend has done so in NewFrame is what
       makes the tool cursors win over the arrow inside the viewport. */
    glfwSetCursor(m_window, cursor);
}

/* ---- skin assets -------------------------------------------------------- */

void App::loadSkinGraphics() {
    if (m_editor.skinsPath.empty()) {
        return;
    }
    const std::filesystem::path sheet =
        std::filesystem::path(m_editor.skinsPath) / "tool_gfx.bmp";
    const Image img = loadImage(sheet.string(), true);
    if (!img.valid()) {
        return;
    }
    m_toolSheet = uploadTexture(img);
    /* The shipped sheet is 96x448: three 32-pixel columns (normal, hover,
       selected) and one 32-pixel row per tool (frmTools.frm:395-425).  It is
       measured rather than assumed so that a skin with more tools still
       indexes correctly. */
    m_toolSheetCols = std::max(1, img.width / 32);
    m_toolSheetRows = std::max(1, img.height / 32);
}

void App::loadCursors() {
    if (m_editor.skinsPath.empty()) {
        return;
    }
    /* The cursor files, in TOOL_* order.  Names are the ones in
       skins/<skin>/cursors/ (modConfig.bas LoadSkin). */
    static const char* kCursorFiles[TOOL_COUNT] = {
        "move.cur", "create.cur", "vselect.cur", "pselect.cur", "vcolor.cur",
        "pcolor.cur", "texture.cur", "scenery.cur", "waypoint.cur",
        "objects.cur", "colorpicker.cur", "sketch.cur", "light.cur",
        "depthmap.cur",
        "hand.cur", "vseladd.cur", "vselsub.cur", "pseladd.cur", "pselsub.cur",
        "scale.cur", "rotate.cur", "connect.cur", "quad.cur", "pixpicker.cur",
        "litpicker.cur", "eraser.cur", "smudge.cur",
    };

    const std::filesystem::path dir =
        std::filesystem::path(m_editor.skinsPath) / "cursors";
    for (int i = 0; i < TOOL_COUNT; ++i) {
        int hx = 0, hy = 0;
        const Image img =
            loadCursor((dir / kCursorFiles[i]).string(), hx, hy);
        if (!img.valid()) {
            continue;
        }
        GLFWimage gi;
        gi.width = img.width;
        gi.height = img.height;
        gi.pixels = const_cast<unsigned char*>(img.rgba.data());
        m_cursors[static_cast<size_t>(i)] = glfwCreateCursor(&gi, hx, hy);
    }
    m_panCursor = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
}

unsigned int App::previewTexture(const std::string& filename, int& w, int& h) {
    w = 0;
    h = 0;
    if (filename.empty()) {
        return 0;
    }
    const unsigned int id = m_editor.texMgr.loadTexture(filename, false);
    if (id != 0) {
        m_editor.texMgr.getSize(id, w, h);
    }
    return id;
}

PendingPopup App::takePendingPopup() {
    const PendingPopup p = m_pendingPopup;
    m_pendingPopup = PendingPopup::None;
    return p;
}

}  // namespace pw
