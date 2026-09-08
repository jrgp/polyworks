#include "color_key.h"

#include <cstddef>

void applyColorKey(unsigned char* rgba, int w, int h) {
    if (rgba == nullptr || w <= 0 || h <= 0) {
        return;
    }

    const std::size_t count =
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    for (std::size_t i = 0; i < count; ++i) {
        unsigned char* px = rgba + i * 4;
        if (px[0] == 0 && px[1] == 255 && px[2] == 0 && px[3] == 255) {
            px[3] = 0;
            /* Zero the green channel too so linear filtering can't bleed green
               fringes in from fully transparent texels. */
            px[1] = 0;
        }
    }
}
