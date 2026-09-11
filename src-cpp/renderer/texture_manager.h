#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

typedef unsigned int GLuint;

class TextureManager {
public:
    void setBasePath(const std::string& path);
    void addSearchPath(const std::string& path);
    /* Drop a path added earlier, so that repointing the game directory does
       not leave the previous one behind to be searched forever. */
    void removeSearchPath(const std::string& path);

    /* fallbackToNotFound: when the file cannot be resolved, return the
       notfound.bmp placeholder rather than 0.  The original does that for
       scenery (frm:2083) but *not* for the map texture: SetMapTexture
       (frm:4279) swallows the error and leaves mapTexture unset, so polygons
       fall back to their vertex colours.  Substituting the placeholder there
       covers the whole map in "missing image" crosses and hides the map. */
    GLuint loadTexture(const std::string& filename,
                       bool fallbackToNotFound = true);
    void getSize(GLuint texId, int& w, int& h) const;
    void clear();
    GLuint getNotFoundTexture();

    /* Resolves a map's texture/scenery filename against the configured search
       paths without uploading it, so the GUI can display the same image the
       renderer uses.  Returns an empty string when nothing matches. */
    std::string resolvePath(const std::string& filename) const { return findFile(filename); }

    /* Number of configured search paths; addSearchPath() ignores duplicates. */
    std::size_t searchPathCount() const { return m_searchPaths.size(); }

    ~TextureManager();

private:
    struct TexEntry {
        GLuint id = 0;
        int w = 0;
        int h = 0;
    };

    std::unordered_map<std::string, TexEntry> m_cache;
    std::vector<std::string> m_searchPaths;
    /* Names already known to be unresolvable.  Without this the renderer
       re-scans every search directory, and prints the diagnostic, once per
       frame.  Cleared whenever the search paths change, since that is exactly
       when a name might start resolving. */
    std::unordered_set<std::string> m_missing;
    GLuint m_notFoundTexId = 0;

    GLuint uploadTexture(const uint8_t* rgba, int w, int h);
    std::string findFile(const std::string& filename) const;
};
