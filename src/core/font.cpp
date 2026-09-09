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

// Same hinting as kGlyphLoadFlags but WITHOUT FT_LOAD_RENDER: computes the glyph
// advance without rasterizing. Used for width measurement (the render pre-pass
// and measureWidth), which previously rasterized every glyph an extra time.
constexpr int kMeasureLoadFlags = FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME |
                                  FT_LOAD_FORCE_AUTOHINT;

// FT_LOAD_TARGET_LCD asks for horizontal RGB-subpixel rendering. The resulting
// bitmap has FT_PIXEL_MODE_LCD with width tripled (one byte per subpixel).
constexpr int kLCDRenderFlags = FT_LOAD_RENDER | FT_LOAD_TARGET_LCD | FT_LOAD_FORCE_AUTOHINT;

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

Font::Font(const uint8_t* data, size_t size, int ptsize)
{
    // Non-throwing: leave the Font !isValid() on any failure so callers can
    // fall back to another source.
    if (!data || size == 0) {
        Debug::log("font", "Font(memory): empty buffer");
        return;
    }
    m_data.assign(data, data + size);
    if (FT_New_Memory_Face(TrueType::getLibrary(), m_data.data(),
                           static_cast<FT_Long>(m_data.size()), 0, &m_face)) {
        Debug::log("font", "FT_New_Memory_Face failed");
        m_face = nullptr;
        return;
    }
    FT_Set_Pixel_Sizes(m_face, 0, ptsize);
    Debug::log("font", "Font(memory): loaded ", size, " bytes at ptsize ", ptsize);
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
        // Advance-only load (no rasterization) for the width sum; the glyphs are
        // rasterized once below in the composition loop.
        if (FT_Load_Char(m_face, codepoint, kMeasureLoadFlags)) {
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

int Font::glyphAdvance(uint32_t codepoint)
{
    auto it = m_advance_cache.find(codepoint);
    if (it != m_advance_cache.end()) {
        return it->second;
    }
    // A glyph that fails to load contributes nothing, matching the previous
    // behaviour of skipping it. Cache that too, so it is not retried.
    int advance = 0;
    if (FT_Load_Char(m_face, codepoint, kMeasureLoadFlags) == 0) {
        advance = m_face->glyph->advance.x >> 6;
    }
    m_advance_cache.emplace(codepoint, advance);
    return advance;
}

const Font::GlyphBitmap& Font::renderedGlyph(uint32_t codepoint)
{
    auto it = m_glyph_cache.find(codepoint);
    if (it != m_glyph_cache.end()) {
        return it->second;
    }
    // Still in the previous generation: promote it rather than rasterise again.
    auto prev = m_glyph_prev.find(codepoint);
    if (prev != m_glyph_prev.end()) {
        auto moved = m_glyph_cache.emplace(codepoint, std::move(prev->second)).first;
        m_glyph_prev.erase(prev);
        return moved->second;
    }
    if (m_glyph_cache.size() >= kGlyphCacheMax) {
        m_glyph_prev = std::move(m_glyph_cache);
        m_glyph_cache.clear();
    }

    GlyphBitmap glyph;
    if (m_face && FT_Load_Char(m_face, codepoint, kLCDRenderFlags) == 0) {
        const FT_GlyphSlot slot = m_face->glyph;
        glyph.left = slot->bitmap_left;
        glyph.top = slot->bitmap_top;
        glyph.advance = slot->advance.x >> 6;
        glyph.rows = static_cast<int>(slot->bitmap.rows);
        glyph.lcd = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_LCD);
        glyph.valid = true;

        if (glyph.lcd) {
            glyph.width = static_cast<int>(slot->bitmap.width) / 3;
            glyph.coverage.resize(static_cast<std::size_t>(glyph.width) * glyph.rows * 3);
            for (int row = 0; row < glyph.rows; ++row) {
                // pitch is signed: a negative one means the rows run upward in
                // memory, so it has to be applied rather than assumed positive.
                const auto* src = slot->bitmap.buffer + row * slot->bitmap.pitch;
                std::memcpy(&glyph.coverage[static_cast<std::size_t>(row) * glyph.width * 3],
                            src, static_cast<std::size_t>(glyph.width) * 3);
            }
        } else {
            // Some faces or builds do not honour the LCD target; normalise the
            // mono/grey bitmap to one coverage byte per pixel while caching, so
            // the drawing loop does not have to care which it got.
            glyph.width = static_cast<int>(slot->bitmap.width);
            glyph.coverage.resize(static_cast<std::size_t>(glyph.width) * glyph.rows);
            for (int row = 0; row < glyph.rows; ++row) {
                for (int col = 0; col < glyph.width; ++col) {
                    glyph.coverage[static_cast<std::size_t>(row) * glyph.width + col] =
                        getGlyphCoverage(slot->bitmap, static_cast<unsigned>(row),
                                         static_cast<unsigned>(col));
                }
            }
        }
    }

    return m_glyph_cache.emplace(codepoint, std::move(glyph)).first->second;
}

int Font::measureWidth(const std::string& utf8_text)
{
    if (!m_face) {
        return 0;
    }
    // Decoded in place rather than through UTF8Util::toCodepoints, which
    // allocates a vector per call. Word-wrapping measures every word of every
    // line, so that was thousands of allocations for a page of text; the
    // decoding itself is the same function either way.
    int width = 0;
    const auto* data = reinterpret_cast<const uint8_t*>(utf8_text.data());
    std::size_t i = 0;
    while (i < utf8_text.size()) {
        std::size_t consumed = 0;
        const uint32_t codepoint =
            UTF8Util::decodeCodepoint(data + i, utf8_text.size() - i, consumed);
        width += glyphAdvance(codepoint);
        i += consumed;
    }
    return width;
}

int Font::measureWidth(const TagLib::String& text)
{
    return measureWidth(text.to8Bit(true));
}

std::unique_ptr<Surface> Font::RenderLCD(const TagLib::String& text,
                                         uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
                                         uint8_t bg_r, uint8_t bg_g, uint8_t bg_b)
{
    if (!m_face) {
        return nullptr;
    }

    int width = 0;
    int font_height = (m_face->size->metrics.height) >> 6;
    int baseline = (m_face->size->metrics.ascender) >> 6;
    int descender = (m_face->size->metrics.descender) >> 6;
    if (baseline - descender > font_height) {
        font_height = baseline - descender;
    }

    // Both passes read the glyph cache, so FreeType rasterises each glyph once
    // per font rather than once per call.
    const std::vector<uint32_t> codepoints = toRenderableCodepoints(text);
    for (uint32_t codepoint : codepoints) {
        width += renderedGlyph(codepoint).advance;
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
        const GlyphBitmap& glyph = renderedGlyph(codepoint);
        if (!glyph.valid) {
            continue;
        }

        const int y_pos = baseline - glyph.top;
        const int x_pos = pen_x + glyph.left;

        // Coverage is cached rather than finished pixels, so the blend against
        // the caller's colours happens here. Subpixel coverage carries one
        // value per channel; the grey fallback carries one for all three.
        for (int row = 0; row < glyph.rows; ++row) {
            const uint8_t* src = glyph.coverage.data()
                               + static_cast<std::size_t>(row) * glyph.width * (glyph.lcd ? 3 : 1);
            for (int col = 0; col < glyph.width; ++col) {
                const uint8_t cR = glyph.lcd ? src[col * 3 + 0] : src[col];
                const uint8_t cG = glyph.lcd ? src[col * 3 + 1] : src[col];
                const uint8_t cB = glyph.lcd ? src[col * 3 + 2] : src[col];
                if ((cR | cG | cB) == 0) {
                    continue;
                }
                const uint8_t out_r = static_cast<uint8_t>((fg_r * cR + bg_r * (255 - cR)) / 255);
                const uint8_t out_g = static_cast<uint8_t>((fg_g * cG + bg_g * (255 - cG)) / 255);
                const uint8_t out_b = static_cast<uint8_t>((fg_b * cB + bg_b * (255 - cB)) / 255);
                sfc->pixel(x_pos + col, y_pos + row, out_r, out_g, out_b, 255);
            }
        }

        pen_x += glyph.advance;
    }
    return sfc;
}

bool Font::isValid()
{
    return m_face != nullptr;
}

} // namespace PsyMP3::Core
