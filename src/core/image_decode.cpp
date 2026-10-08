/*
 * image_decode.cpp - JPEG and PNG decoding, by way of the vendored stb_image
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * stb_image decodes album art, which comes out of whatever file is being
 * played and so is not to be trusted. Its implementation is compiled here
 * and nowhere else, with every stb symbol static, so the rest of the program
 * sees only the two functions in core/image_decode.h. Unlike stb_vorbis it is
 * valid C++ and keeps to its own stbi prefix, so it needs no object of its
 * own: this file is an ordinary member of the core library, and of
 * psymp3.final.cpp in the --enable-final build.
 */

#include "psymp3.h"

/* Cover art is JPEG or PNG. The other decoders stb_image carries (BMP, GIF,
 * PSD, TGA, HDR, PIC, PNM) would only be more code for a hostile file to
 * reach, so they are not compiled. */
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG

/* Images arrive as bytes from the tag reader, never as a path. */
#define STBI_NO_STDIO

#define STBI_MAX_DIMENSIONS PSYMP3_IMAGE_MAX_DIMENSION

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION

/* Upstream's code, kept byte-identical, is not held to the tree's warning
 * policy. The first two keep a compiler quiet about names in this list that
 * only the other one knows. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif

#include "../../third_party/stb/stb_image.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

unsigned char *psymp3_image_decode_rgba(const unsigned char *data, size_t size,
                                        int *width, int *height)
{
    if (!data || size == 0 || size > static_cast<size_t>(INT_MAX) || !width || !height) {
        return nullptr;
    }

    int w = 0;
    int h = 0;
    int channels_in_file = 0;
    unsigned char *pixels =
        stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels_in_file, 4);
    if (!pixels) {
        return nullptr;
    }
    if (w <= 0 || h <= 0) {
        stbi_image_free(pixels);
        return nullptr;
    }

    *width = w;
    *height = h;
    return pixels;
}

void psymp3_image_free(unsigned char *pixels)
{
    stbi_image_free(pixels);
}
