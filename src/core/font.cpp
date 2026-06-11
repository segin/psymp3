/*
 * font.cpp - FreeType-backed font wrapper, class implementation.
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

namespace PsyMP3::Core {

namespace {

constexpr int kGlyphLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_MONO |
                                FT_LOAD_MONOCHROME | FT_LOAD_FORCE_AUTOHINT;

std::vector<uint32_t> toRenderableCodepoints(const TagLib::String& text)
{
    return UTF8Util::toCodepoints(text.to8Bit(true));
}

unsigned char getGlyphCoverage(const FT_Bitmap& bitmap, unsigned int row, unsigned int col)
{
    if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
        const unsigned char byte = bitmap.buffer[row * bitmap.pitch + (col / 8)];
        const unsigned char mask = static_cast<unsigned char>(0x80 >> (col % 8));
        return (byte & mask) ? 255 : 0;
    }

    if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
        return bitmap.buffer[row * bitmap.pitch + col];
    }

    return 0;
}

} // namespace

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

Font::~Font()
{
    Debug::log("font", "Font destructor called.");
    FT_Done_Face(m_face);
}

std::unique_ptr<Surface> Font::Render(const TagLib::String& text, uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_face) {
        Debug::log("font", "Font::Render: m_face is null.");
        return nullptr;
    }

    int width = 0;
    int font_height = (m_face->size->metrics.height) >> 6;
    int baseline = (m_face->size->metrics.ascender) >> 6;
    int descender = (m_face->size->metrics.descender) >> 6;
    
    if (baseline - descender > font_height) {
        font_height = baseline - descender;
    }

    const std::vector<uint32_t> codepoints = toRenderableCodepoints(text);
    for (uint32_t codepoint : codepoints) {
        if (FT_Load_Char(m_face, codepoint, kGlyphLoadFlags)) {
            Debug::log("font", "FT_Load_Char failed for codepoint: ", codepoint);
            continue;
        }
        width += m_face->glyph->advance.x >> 6;
    }

    // Clamp the surface width: text comes from untrusted tags and could
    // otherwise drive a multi-gigabyte allocation. The visible/scrolled area is
    // far smaller, and glyphs past the clamp are clipped by Surface::pixel.
    static constexpr int MAX_TEXT_SURFACE_WIDTH = 8192;
    if (width > MAX_TEXT_SURFACE_WIDTH) {
        width = MAX_TEXT_SURFACE_WIDTH;
    }

    if (width <= 0 || font_height <= 0) {
        return std::make_unique<Surface>(1, 1);
    }

    auto sfc = std::make_unique<Surface>(width, font_height, true);
    if (!sfc) {
        Debug::log("font", "Failed to create surface for text rendering.");
        return nullptr;
    }

    SDL_SetSurfaceBlendMode(sfc->getHandle(), SDL_BLENDMODE_BLEND);
    sfc->FillRect(sfc->MapRGBA(0, 0, 0, 0));

    int pen_x = 0;
    for (uint32_t codepoint : codepoints) {
        if (FT_Load_Char(m_face, codepoint, kGlyphLoadFlags)) {
            continue;
        }

        FT_GlyphSlot slot = m_face->glyph;
        int y_pos = baseline - slot->bitmap_top;
        int x_pos = pen_x + slot->bitmap_left;

        for (unsigned int row = 0; row < slot->bitmap.rows; ++row) {
            for (unsigned int col = 0; col < slot->bitmap.width; ++col) {
                unsigned char alpha = getGlyphCoverage(slot->bitmap, row, col);
                if (alpha > 0) {
                    sfc->pixel(x_pos + col, y_pos + row, r, g, b, alpha);
                }
            }
        }

        pen_x += slot->advance.x >> 6;
    }
    return sfc;
}

std::unique_ptr<Surface> Font::RenderLCD(const TagLib::String& text,
                                         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
                                         uint8_t bg_r, uint8_t bg_g, uint8_t bg_b)
{
    if (!m_face) {
        return nullptr;
    }

    // FT_LOAD_TARGET_LCD asks for horizontal RGB-subpixel rendering. The
    // resulting bitmap has FT_PIXEL_MODE_LCD with width tripled (one byte per
    // subpixel: R, G, B per logical pixel).
    constexpr int kLCDLoadFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_LCD | FT_LOAD_FORCE_AUTOHINT;

    int width = 0;
    int font_height = (m_face->size->metrics.height) >> 6;
    int baseline = (m_face->size->metrics.ascender) >> 6;
    int descender = (m_face->size->metrics.descender) >> 6;
    if (baseline - descender > font_height) {
        font_height = baseline - descender;
    }

    const std::vector<uint32_t> codepoints = toRenderableCodepoints(text);
    for (uint32_t codepoint : codepoints) {
        if (FT_Load_Char(m_face, codepoint, kLCDLoadFlags)) {
            continue;
        }
        width += m_face->glyph->advance.x >> 6;
    }

    // Clamp the surface width: text comes from untrusted tags and could
    // otherwise drive a multi-gigabyte allocation. The visible/scrolled area is
    // far smaller, and glyphs past the clamp are clipped by Surface::pixel.
    static constexpr int MAX_TEXT_SURFACE_WIDTH = 8192;
    if (width > MAX_TEXT_SURFACE_WIDTH) {
        width = MAX_TEXT_SURFACE_WIDTH;
    }

    if (width <= 0 || font_height <= 0) {
        return std::make_unique<Surface>(1, 1);
    }

    auto sfc = std::make_unique<Surface>(width, font_height, true);
    if (!sfc) {
        return nullptr;
    }

    // The LCD path produces fully-opaque output: every pixel is the bg color
    // unless overdrawn by a glyph. Start by painting bg over the bbox so
    // pixels not touched by any glyph match the surrounding fill.
    SDL_SetSurfaceBlendMode(sfc->getHandle(), SDL_BLENDMODE_BLEND);
    sfc->FillRect(sfc->MapRGBA(bg_r, bg_g, bg_b, 255));

    int pen_x = 0;
    for (uint32_t codepoint : codepoints) {
        if (FT_Load_Char(m_face, codepoint, kLCDLoadFlags)) {
            continue;
        }

        FT_GlyphSlot slot = m_face->glyph;
        const int y_pos = baseline - slot->bitmap_top;
        const int x_pos = pen_x + slot->bitmap_left;

        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_LCD) {
            const unsigned int glyph_pixels_w = slot->bitmap.width / 3;
            for (unsigned int row = 0; row < slot->bitmap.rows; ++row) {
                const auto* row_ptr = slot->bitmap.buffer + row * slot->bitmap.pitch;
                for (unsigned int col = 0; col < glyph_pixels_w; ++col) {
                    const uint8_t cR = row_ptr[col * 3 + 0];
                    const uint8_t cG = row_ptr[col * 3 + 1];
                    const uint8_t cB = row_ptr[col * 3 + 2];
                    if ((cR | cG | cB) == 0) {
                        continue;
                    }
                    const uint8_t out_r = static_cast<uint8_t>((fg_r * cR + bg_r * (255 - cR)) / 255);
                    const uint8_t out_g = static_cast<uint8_t>((fg_g * cG + bg_g * (255 - cG)) / 255);
                    const uint8_t out_b = static_cast<uint8_t>((fg_b * cB + bg_b * (255 - cB)) / 255);
                    sfc->pixel(x_pos + col, y_pos + row, out_r, out_g, out_b, 255);
                }
            }
        } else {
            // Fallback for fonts/configurations where the LCD target wasn't
            // honored: treat the bitmap as grayscale or mono coverage and
            // blend uniformly against the bg.
            for (unsigned int row = 0; row < slot->bitmap.rows; ++row) {
                for (unsigned int col = 0; col < slot->bitmap.width; ++col) {
                    const uint8_t cov = getGlyphCoverage(slot->bitmap, row, col);
                    if (cov == 0) {
                        continue;
                    }
                    const uint8_t out_r = static_cast<uint8_t>((fg_r * cov + bg_r * (255 - cov)) / 255);
                    const uint8_t out_g = static_cast<uint8_t>((fg_g * cov + bg_g * (255 - cov)) / 255);
                    const uint8_t out_b = static_cast<uint8_t>((fg_b * cov + bg_b * (255 - cov)) / 255);
                    sfc->pixel(x_pos + col, y_pos + row, out_r, out_g, out_b, 255);
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

} // namespace PsyMP3::Core
