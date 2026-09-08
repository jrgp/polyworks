#include "texture_manager.h"

#include "color_key.h"
#include "stb_image.h"

#include <cstdio>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

/* PW_RENDERER_NO_OPENGL lets the headless test binary compile the asset
   resolver -- which is pure std::filesystem -- without linking a GL library. */
#if defined(PW_RENDERER_NO_OPENGL)
#define PW_RENDERER_HAS_OPENGL 0
#elif defined(__has_include)
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

namespace fs = std::filesystem;

std::string toLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string normalizePathKey(const std::string& value) {
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return toLowerCopy(normalized);
}

fs::path normalizeInputPath(const std::string& value) {
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return fs::path(normalized).lexically_normal();
}

bool isSameName(const fs::path& candidate, const std::string& wantedLower) {
    return toLowerCopy(candidate.filename().string()) == wantedLower;
}

bool resolveRelativeCaseInsensitive(const fs::path& base,
                                    const fs::path& relative,
                                    fs::path& resolved) {
    std::error_code ec;
    fs::path current = base;
    for (const auto& part : relative) {
        const std::string name = part.string();
        if (name.empty() || name == ".") {
            continue;
        }
        if (name == "..") {
            current = current.parent_path();
            continue;
        }

        const fs::path exact = current / part;
        if (fs::exists(exact, ec)) {
            current = exact;
            continue;
        }

        if (!fs::is_directory(current, ec)) {
            return false;
        }

        const std::string wantedLower = toLowerCopy(name);
        bool matched = false;
        for (const auto& entry : fs::directory_iterator(current, ec)) {
            if (ec) {
                return false;
            }
            if (isSameName(entry.path(), wantedLower)) {
                current = entry.path();
                matched = true;
                break;
            }
        }

        if (!matched) {
            return false;
        }
    }

    resolved = current;
    return true;
}

bool resolveAnyPathCaseInsensitive(const std::string& value, fs::path& resolved) {
    std::error_code ec;
    const fs::path input = normalizeInputPath(value);
    if (input.empty()) {
        return false;
    }

    if (input.is_absolute()) {
        return resolveRelativeCaseInsensitive(input.root_path(), input.relative_path(), resolved) &&
               fs::exists(resolved, ec);
    }

    return resolveRelativeCaseInsensitive(fs::current_path(ec), input, resolved) && fs::exists(resolved, ec);
}

bool resolveFromBaseCaseInsensitive(const std::string& basePath,
                                    const std::string& filename,
                                    fs::path& resolved) {
    std::error_code ec;
    fs::path baseResolved;
    if (!resolveAnyPathCaseInsensitive(basePath, baseResolved) || !fs::is_directory(baseResolved, ec)) {
        return false;
    }

    fs::path candidate;
    return resolveRelativeCaseInsensitive(baseResolved, normalizeInputPath(filename), candidate) &&
           fs::is_regular_file(candidate, ec) &&
           (resolved = candidate, true);
}

}  // namespace

void TextureManager::setBasePath(const std::string& path) {
    m_searchPaths.clear();
    if (path.empty()) {
        return;
    }

    m_searchPaths.push_back(path);

    std::error_code ec;
    const fs::path parent = normalizeInputPath(path).parent_path();
    if (!parent.empty() && parent.string() != normalizeInputPath(path).string() && fs::exists(parent, ec)) {
        m_searchPaths.push_back(parent.string());
    }
}

void TextureManager::addSearchPath(const std::string& path) {
    if (path.empty()) {
        return;
    }

    const std::string key = normalizePathKey(path);
    for (const auto& existing : m_searchPaths) {
        if (normalizePathKey(existing) == key) {
            return;
        }
    }

    m_searchPaths.insert(m_searchPaths.begin(), path);
}

GLuint TextureManager::loadTexture(const std::string& filename) {
    if (filename.empty()) {
        return 0;
    }

    const std::string cacheKey = normalizePathKey(filename);
    const auto it = m_cache.find(cacheKey);
    if (it != m_cache.end()) {
        return it->second.id;
    }

    const std::string path = findFile(filename);
    if (path.empty()) {
        /* The audit brief requires useful diagnostics when assets can't be
           resolved; the original silently substituted notfound.bmp, which made
           a missing Soldat directory very hard to diagnose. */
        std::string searched;
        for (const auto& sp : m_searchPaths) {
            if (!searched.empty()) searched += ", ";
            searched += sp;
        }
        std::fprintf(stderr,
                     "PolyWorks: asset not found: \"%s\" (searched: %s)\n",
                     filename.c_str(),
                     searched.empty() ? "<no search paths configured>"
                                      : searched.c_str());
        const GLuint notFoundId = getNotFoundTexture();
        if (notFoundId != 0) {
            int notFoundW = 1;
            int notFoundH = 1;
            getSize(notFoundId, notFoundW, notFoundH);
            m_cache.emplace(cacheKey, TexEntry{notFoundId, notFoundW, notFoundH});
        }
        return notFoundId;
    }

    int w = 0;
    int h = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (pixels == nullptr) {
        return 0;
    }

    applyColorKey(pixels, w, h);
    const GLuint texId = uploadTexture(pixels, w, h);
    stbi_image_free(pixels);
    if (texId == 0) {
        return 0;
    }

    m_cache.emplace(cacheKey, TexEntry{texId, w, h});
    return texId;
}

void TextureManager::getSize(GLuint texId, int& w, int& h) const {
    w = 0;
    h = 0;

    for (const auto& [name, entry] : m_cache) {
        if (entry.id == texId) {
            w = entry.w;
            h = entry.h;
            return;
        }
    }

    if (texId != 0 && texId == m_notFoundTexId) {
        w = 1;
        h = 1;
    }
}

void TextureManager::clear() {
#if PW_RENDERER_HAS_OPENGL
    std::unordered_set<GLuint> uniqueIds;
    for (const auto& [name, entry] : m_cache) {
        if (entry.id != 0) {
            uniqueIds.insert(entry.id);
        }
    }
    if (m_notFoundTexId != 0) {
        uniqueIds.insert(m_notFoundTexId);
    }

    std::vector<GLuint> ids(uniqueIds.begin(), uniqueIds.end());
    if (!ids.empty()) {
        glDeleteTextures(static_cast<GLsizei>(ids.size()), ids.data());
    }
#endif

    m_cache.clear();
    m_notFoundTexId = 0;
}

GLuint TextureManager::getNotFoundTexture() {
    if (m_notFoundTexId != 0) {
        return m_notFoundTexId;
    }

    const std::string cacheKey = normalizePathKey("notfound.bmp");
    const auto cached = m_cache.find(cacheKey);
    if (cached != m_cache.end() && cached->second.id != 0) {
        m_notFoundTexId = cached->second.id;
        return m_notFoundTexId;
    }

    const std::string path = findFile("notfound.bmp");
    if (!path.empty()) {
        int w = 0;
        int h = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (pixels != nullptr) {
            applyColorKey(pixels, w, h);
            m_notFoundTexId = uploadTexture(pixels, w, h);
            stbi_image_free(pixels);
            if (m_notFoundTexId != 0) {
                m_cache[cacheKey] = TexEntry{m_notFoundTexId, w, h};
                return m_notFoundTexId;
            }
        }
    }

    std::fprintf(stderr,
                 "PolyWorks: could not load the notfound.bmp placeholder from "
                 "the skins directory; falling back to flat magenta.\n");
    const uint8_t magenta[4] = {255, 0, 255, 255};
    m_notFoundTexId = uploadTexture(magenta, 1, 1);
    return m_notFoundTexId;
}

TextureManager::~TextureManager() = default;

GLuint TextureManager::uploadTexture(const uint8_t* rgba, int w, int h) {
#if PW_RENDERER_HAS_OPENGL
    if (rgba == nullptr || w <= 0 || h <= 0) {
        return 0;
    }

    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0) {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    /* VB6 never sets D3DTSS_ADDRESSU/V, so Direct3D's default
       D3DTADDRESS_WRAP applies: map textures tile.  Real Soldat maps depend on
       this - 99% of vertices in the shipped maps have texture coordinates
       outside [0,1] (measured range +-12.75 across maps/) - so clamping
       smeared a single edge texel across whole polygons. */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 w,
                 h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba);
    return texId;
#else
    (void)rgba;
    (void)w;
    (void)h;
    return 0;
#endif
}

std::string TextureManager::findFile(const std::string& filename) const {
    if (filename.empty()) {
        return {};
    }

    std::error_code ec;
    fs::path resolved;
    if (resolveAnyPathCaseInsensitive(filename, resolved) && fs::is_regular_file(resolved, ec)) {
        return resolved.string();
    }

    for (const auto& searchPath : m_searchPaths) {
        if (resolveFromBaseCaseInsensitive(searchPath, filename, resolved)) {
            return resolved.string();
        }
    }

    return {};
}
