/*
 * font.cpp - wrapper for SDL_ttf's TTF_Font type, class implementation.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

/**
 * @brief Constructs a Font object by loading a TrueType or OpenType font face.
 *
 * Opens the font file at the specified path using the FreeType library and sets
 * the requested pixel size. Throws `std::runtime_error` if the font cannot be loaded.
 *
 * @param file The filesystem path to the font file.
 * @param ptsize The desired glyph pixel height.
 */
Font::Font(const TagLib::String& file, int ptsize)
{
    Debug::log("font", "Font constructor called for file: ", file.to8Bit(true), ", ptsize: ", ptsize);
    if (FT_New_Face(TrueType::getLibrary(), file.toCString(), 0, &m_face)) {
        Debug::log("font", "FT_New_Face failed for font: ", file.to8Bit(true));
        throw std::runtime_error("Failed to load font: " + file.to8Bit(true));
    }
    Debug::log("font", "FT_New_Face successful.");
    FT_Set_Pixel_Sizes(m_face, 0, ptsize);
    Debug::log("font", "FT_Set_Pixel_Sizes successful.");
}

/**
 * @brief Destroys the Font object and releases the underlying FreeType face.
 */
Font::~Font()
{
    Debug::log("font", "Font destructor called.");
    FT_Done_Face(m_face);
}

/**
 * @brief Renders a UTF-8 text string into a new Surface using FreeType.
 *
 * Performs a two-pass rendering: the first pass measures the total advance
 * width of all glyphs; the second pass draws each glyph bitmap into an
 * RGBA surface. The resulting surface uses alpha blending and the supplied
 * RGB colour as the text colour.
 *
 * @param text The text string to render.
 * @param r    Red component of the text colour (0–255).
 * @param g    Green component of the text colour (0–255).
 * @param b    Blue component of the text colour (0–255).
 * @return A new Surface containing the rendered text, or `nullptr` on failure.
 */
std::unique_ptr<Surface> Font::Render(const TagLib::String& text, uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_face) {
        Debug::log("font", "Font::Render: m_face is null.");
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
            Debug::log("font", "FT_Load_Char failed for character: ", *p);
            continue;
        }
        width += m_face->glyph->advance.x >> 6;
    }

    if (width <= 0 || font_height <= 0) {
        // Return an empty but valid surface for empty strings to avoid crashes.
        return std::make_unique<Surface>(1, 1);
    }

    auto sfc = std::make_unique<Surface>(width, font_height, true);
    if (!sfc) {
        Debug::log("font", "Failed to create surface for text rendering.");
        return nullptr;
    }

    // Initialize surface with transparent background
    sfc->FillRect(sfc->MapRGBA(0, 0, 0, 0));

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
                    // Use the FreeType alpha mask to render the text color with proper alpha
                    sfc->pixel(x_pos + col, y_pos + row, r, g, b, alpha);
                }
            }
        }

        pen_x += slot->advance.x >> 6;
    }
    return sfc;
}

/**
 * @brief Returns whether the Font was loaded successfully.
 * @return `true` if the underlying FreeType face is valid, `false` otherwise.
 */
bool Font::isValid()
{
    return m_face != nullptr;
}
