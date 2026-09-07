/*
 * pw_stb_image.c — thin C wrapper around stb_image for PolyWorks.
 *
 * stb_image.h is vendored in this same directory (src-fp/vendor/stb_image.h).
 * MIT / Public Domain — see stb_image.h header for full license text.
 *
 * Build:
 *   cc -O2 -I. -c pw_stb_image.c -o pw_stb_image.o
 *   ar rcs libpw_stb_image.a pw_stb_image.o
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF

#include "stb_image.h"

/* Load an image from file.
 * Returns pixel data as RGBA (4 bytes per pixel), NULL on failure.
 * Caller must free with pw_stb_free(). */
unsigned char* pw_stb_load(const char* filename, int* width, int* height)
{
    int channels;
    return stbi_load(filename, width, height, &channels, 4);
}

/* Load first frame of a GIF. */
unsigned char* pw_stb_load_gif_first_frame(const char* filename, int* width, int* height)
{
    int channels;
    stbi_set_flip_vertically_on_load(0);
    return stbi_load(filename, width, height, &channels, 4);
}

void pw_stb_free(unsigned char* data)
{
    stbi_image_free(data);
}

const char* pw_stb_failure_reason(void)
{
    return stbi_failure_reason();
}
