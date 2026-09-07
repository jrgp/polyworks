/*
 * stb_image implementation unit — compiled once here so all other TUs
 * include stb_image.h without the implementation define.
 */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_BMP
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#include "stb_image.h"
