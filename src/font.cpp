/*
 * font.cpp - wrapper for SDL_ttf's TTF_Font type, class implementation.
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

#include "psymp3.h"

Font::Font(const TagLib::String& file, int ptsize)
{
    std::cout << "Font constructor called for file: " << file.to8Bit(true) << ", ptsize: " << ptsize << std::endl;
    if (FT_New_Face(TrueType::getLibrary(), file.toCString(), 0, &m_face)) {
        std::cerr << "FT_New_Face failed for font: " << file.to8Bit(true) << std::endl;
        throw std::runtime_error("Failed to load font: " + file.to8Bit(true));
    }
    std::cout << "FT_New_Face successful." << std::endl;
    FT_Set_Pixel_Sizes(m_face, 0, ptsize);
    std::cout << "FT_Set_Pixel_Sizes successful." << std::endl;
}

Font::~Font()
{
    std::cout << "Font destructor called." << std::endl;
    FT_Done_Face(m_face);
}

std::unique_ptr<Surface> Font::Render(const TagLib::String& text, uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_face) {
        std::cerr << "Font::Render: m_face is null." << std::endl;
        return nullptr;
    }

    // --- Calculate text dimensions ---
    int width = 0;
    // Use font-wide metrics for consistent height and baseline.
    // The values are in 1/64th of a pixel, so we shift.
    int font_height = (m_face->size->metrics.height) >> 6;
    int baseline = (m_face->size->metrics.ascender) >> 6;

    std::string text_utf8 = text.to8Bit(true);
    for (const char* p = text_utf8.c_str(); *p; p++) {
        if (FT_Load_Char(m_face, *p, FT_LOAD_RENDER)) {
            std::cerr << "FT_Load_Char failed for character: " << *p << std::endl;
            continue;
        }
        width += m_face->glyph->advance.x >> 6;
    }

    if (width <= 0 || font_height <= 0) {
        // Return an empty but valid surface for empty strings to avoid crashes.
        return std::make_unique<Surface>(1, 1);
    }

    auto sfc = std::make_unique<Surface>(width, font_height);
    if (!sfc) {
        std::cerr << "Failed to create surface for text rendering." << std::endl;
        return nullptr;
    }

    

    int pen_x = 0;
    for (const char* p = text_utf8.c_str(); *p; p++) {
        if (FT_Load_Char(m_face, *p, FT_LOAD_RENDER)) {
            continue;
        }

        FT_GlyphSlot slot = m_face->glyph;

        // Calculate the correct top-left position for this glyph's bitmap.
        int y_pos = baseline - slot->bitmap_top;
        int x_pos = pen_x + slot->bitmap_left;

        for (unsigned int row = 0; row < slot->bitmap.rows; ++row) {
            for (unsigned int col = 0; col < slot->bitmap.width; ++col) {
                unsigned char alpha = slot->bitmap.buffer[row * slot->bitmap.pitch + col];
                if (alpha > 0) {
                    // The glyph bitmap from FreeType is an alpha mask. We just need to
                    // draw the requested text color with the alpha from the mask.
                    // Pre-multiplication is not needed for standard SDL alpha blending.
                    sfc->pixel(x_pos + col, y_pos + row, r, g, b, alpha);
                }
            }
        }

        pen_x += slot->advance.x >> 6;
    }
    return sfc;
}

bool Font::isValid()
{
    return m_face != nullptr;
}
