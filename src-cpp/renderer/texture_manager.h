#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

typedef unsigned int GLuint;

class TextureManager {
public:
    void setBasePath(const std::string& path);
    void addSearchPath(const std::string& path);

    GLuint loadTexture(const std::string& filename);
    void getSize(GLuint texId, int& w, int& h) const;
    void clear();
    GLuint getNotFoundTexture();

    ~TextureManager();

private:
    struct TexEntry {
        GLuint id = 0;
        int w = 0;
        int h = 0;
    };

    std::unordered_map<std::string, TexEntry> m_cache;
    std::vector<std::string> m_searchPaths;
    GLuint m_notFoundTexId = 0;

    GLuint uploadTexture(const uint8_t* rgba, int w, int h);
    std::string findFile(const std::string& filename) const;
};
