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

// Confined to this file: font.h keeps these behind void* and std::size_t so
// every translation unit that pulls in psymp3.h does not need their headers.
#include <hb.h>
#include <hb-ft.h>
#include <SheenBidi/SheenBidi.h>

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
    m_ptsize = ptsize;
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
    m_ptsize = ptsize;
    Debug::log("font", "Font(memory): loaded ", size, " bytes at ptsize ", ptsize);
}

Font::~Font()
{
    Debug::log("font", "Font destructor called.");
    for (void* hb : m_hb_fonts) {
        if (hb) {
            hb_font_destroy(static_cast<hb_font_t*>(hb));
        }
    }
    for (FallbackFace& fallback : m_fallbacks) {
        if (fallback.face) {
            FT_Done_Face(fallback.face);
        }
    }
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

bool Font::needsComplexLayout(const std::string& utf8_text)
{
    const auto* data = reinterpret_cast<const uint8_t*>(utf8_text.data());
    std::size_t i = 0;
    while (i < utf8_text.size()) {
        std::size_t consumed = 0;
        const uint32_t cp =
            UTF8Util::decodeCodepoint(data + i, utf8_text.size() - i, consumed);
        i += consumed;
        const bool complex_cp =
               (cp >= 0x0300 && cp <= 0x036F)    // combining marks
            || (cp >= 0x0590 && cp <= 0x1FFF)    // Hebrew, Arabic, Indic, Thai...
            || (cp >= 0x200E && cp <= 0x200F)    // LRM / RLM
            || (cp >= 0x202A && cp <= 0x202E)    // bidi embedding controls
            || (cp >= 0x2066 && cp <= 0x2069)    // bidi isolates
            || (cp >= 0xFB1D && cp <= 0xFEFC)    // Hebrew/Arabic presentation forms
            || (cp >= 0x10800 && cp <= 0x10FFF); // RTL historic scripts
        if (complex_cp) {
            return true;
        }
    }
    return false;
}

void* Font::harfbuzzFont(std::size_t face_index)
{
    if (m_hb_fonts.size() <= face_index) {
        m_hb_fonts.resize(face_index + 1, nullptr);
    }
    if (!m_hb_fonts[face_index]) {
        FT_Face face = (face_index == 0)
            ? m_face
            : (face_index - 1 < m_fallbacks.size() ? m_fallbacks[face_index - 1].face : nullptr);
        if (!face) {
            return nullptr;
        }
        // Referencing rather than taking ownership: the FT_Face outlives this
        // and is freed by the destructor.
        m_hb_fonts[face_index] = hb_ft_font_create_referenced(face);
    }
    return m_hb_fonts[face_index];
}

const Font::GlyphBitmap& Font::shapedGlyph(std::size_t face_index, uint32_t glyph_id)
{
    const uint64_t key = (static_cast<uint64_t>(face_index) << 32) | glyph_id;
    auto it = m_shaped_cache.find(key);
    if (it != m_shaped_cache.end()) {
        return it->second;
    }
    if (m_shaped_cache.size() >= kGlyphCacheMax * 2) {
        m_shaped_cache.clear();
    }

    FT_Face face = (face_index == 0)
        ? m_face
        : (face_index - 1 < m_fallbacks.size() ? m_fallbacks[face_index - 1].face : nullptr);

    GlyphBitmap glyph;
    // Shaping yields glyph ids, so this loads by index rather than character.
    if (face && FT_Load_Glyph(face, glyph_id, kLCDRenderFlags) == 0) {
        const FT_GlyphSlot slot = face->glyph;
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
                const auto* src = slot->bitmap.buffer + row * slot->bitmap.pitch;
                std::memcpy(&glyph.coverage[static_cast<std::size_t>(row) * glyph.width * 3],
                            src, static_cast<std::size_t>(glyph.width) * 3);
            }
        } else {
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
    return m_shaped_cache.emplace(key, std::move(glyph)).first->second;
}

int Font::shapeRuns(const std::string& utf8_text,
                    const std::function<void(std::size_t, uint32_t, int, int)>& emit)
{
    if (!m_face || utf8_text.empty()) {
        return 0;
    }

    // SheenBidi resolves the reading order: which spans of the string are
    // right-to-left, and what order they appear in on screen. Without this a
    // Hebrew or Arabic title draws backwards, however well its glyphs are
    // shaped.
    SBCodepointSequence sequence = { SBStringEncodingUTF8,
                                     const_cast<char*>(utf8_text.data()),
                                     utf8_text.size() };
    SBAlgorithmRef algorithm = SBAlgorithmCreate(&sequence);
    if (!algorithm) {
        return 0;
    }
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(algorithm, 0, INT32_MAX,
                                                          SBLevelDefaultLTR);
    if (!paragraph) {
        SBAlgorithmRelease(algorithm);
        return 0;
    }
    SBLineRef line = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
    if (!line) {
        SBParagraphRelease(paragraph);
        SBAlgorithmRelease(algorithm);
        return 0;
    }

    int pen_x = 0;
    const SBRun* runs = SBLineGetRunsPtr(line);
    const SBUInteger run_count = SBLineGetRunCount(line);

    for (SBUInteger r = 0; r < run_count; ++r) {
        const SBRun& run = runs[r];
        const bool rtl = (run.level & 1) != 0;

        // A run is one direction but may still cross faces -- Latin and CJK in
        // one phrase -- so it is split again wherever the resolved face changes.
        std::size_t pos = run.offset;
        const std::size_t run_end = run.offset + run.length;
        while (pos < run_end) {
            std::size_t consumed = 0;
            const uint32_t first_cp = UTF8Util::decodeCodepoint(
                reinterpret_cast<const uint8_t*>(utf8_text.data()) + pos,
                run_end - pos, consumed);
            const FT_Face want = faceFor(first_cp);
            std::size_t face_index = 0;
            for (std::size_t i = 0; i < m_fallbacks.size(); ++i) {
                if (m_fallbacks[i].face == want) {
                    face_index = i + 1;
                    break;
                }
            }

            std::size_t seg_end = pos + consumed;
            while (seg_end < run_end) {
                std::size_t next = 0;
                const uint32_t cp = UTF8Util::decodeCodepoint(
                    reinterpret_cast<const uint8_t*>(utf8_text.data()) + seg_end,
                    run_end - seg_end, next);
                if (faceFor(cp) != want) {
                    break;
                }
                seg_end += next;
            }

            auto* hb_font = static_cast<hb_font_t*>(harfbuzzFont(face_index));
            if (hb_font) {
                hb_buffer_t* buffer = hb_buffer_create();
                hb_buffer_add_utf8(buffer, utf8_text.data(), static_cast<int>(utf8_text.size()),
                                   static_cast<unsigned>(pos),
                                   static_cast<int>(seg_end - pos));
                hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
                hb_buffer_guess_segment_properties(buffer);
                hb_shape(hb_font, buffer, nullptr, 0);

                unsigned count = 0;
                const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &count);
                const hb_glyph_position_t* gpos = hb_buffer_get_glyph_positions(buffer, &count);
                for (unsigned g = 0; g < count; ++g) {
                    // HarfBuzz works in 26.6 fixed point.
                    emit(face_index, info[g].codepoint,
                         pen_x + (gpos[g].x_offset >> 6), -(gpos[g].y_offset >> 6));
                    pen_x += gpos[g].x_advance >> 6;
                }
                hb_buffer_destroy(buffer);
            }
            pos = seg_end;
        }
    }

    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(algorithm);
    return pen_x;
}

bool Font::addFallback(const TagLib::String& file)
{
    FallbackFace fallback;
    if (FT_New_Face(TrueType::getLibrary(), file.toCString(), 0, &fallback.face)) {
        Debug::log("font", "Font::addFallback: could not load ", file.to8Bit(true));
        return false;
    }
    FT_Set_Pixel_Sizes(fallback.face, 0, m_ptsize);
    m_fallbacks.push_back(std::move(fallback));
    Debug::log("font", "Font::addFallback: added ", file.to8Bit(true),
               " (", (long)m_fallbacks.back().face->num_glyphs, " glyphs)");
    return true;
}

bool Font::addFallback(const uint8_t* data, size_t size)
{
    if (!data || size == 0) {
        return false;
    }
    FallbackFace fallback;
    // FT_New_Memory_Face does not copy, so the buffer has to outlive the face.
    fallback.data.assign(data, data + size);
    if (FT_New_Memory_Face(TrueType::getLibrary(), fallback.data.data(),
                           static_cast<FT_Long>(fallback.data.size()), 0, &fallback.face)) {
        return false;
    }
    FT_Set_Pixel_Sizes(fallback.face, 0, m_ptsize);
    m_fallbacks.push_back(std::move(fallback));
    return true;
}

FT_Face Font::faceFor(uint32_t codepoint) const
{
    if (!m_face) {
        return nullptr;
    }
    if (FT_Get_Char_Index(m_face, codepoint) != 0) {
        return m_face;
    }
    for (const FallbackFace& fallback : m_fallbacks) {
        if (fallback.face && FT_Get_Char_Index(fallback.face, codepoint) != 0) {
            return fallback.face;
        }
    }
    // Nobody has it: the primary draws its .notdef box, as before.
    return m_face;
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
    FT_Face face = faceFor(codepoint);
    if (face && FT_Load_Char(face, codepoint, kMeasureLoadFlags) == 0) {
        advance = face->glyph->advance.x >> 6;
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
    FT_Face face = faceFor(codepoint);
    if (face && FT_Load_Char(face, codepoint, kLCDRenderFlags) == 0) {
        const FT_GlyphSlot slot = face->glyph;
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
    // Complex scripts are measured by laying them out, so wrapping can never
    // disagree with what is drawn -- shaped Arabic is narrower than the sum of
    // its isolated forms, and a wrap computed the other way would be wrong.
    if (needsComplexLayout(utf8_text)) {
        return shapeRuns(utf8_text, [](std::size_t, uint32_t, int, int) {});
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

    // Complex scripts go through SheenBidi and HarfBuzz; everything else keeps
    // the direct codepoint-to-glyph path, which is both faster and renders
    // exactly as it always has.
    const std::string utf8 = text.to8Bit(true);
    const bool complex_layout = needsComplexLayout(utf8);

    // Both passes read the glyph cache, so FreeType rasterises each glyph once
    // per font rather than once per call.
    const std::vector<uint32_t> codepoints =
        complex_layout ? std::vector<uint32_t>() : toRenderableCodepoints(text);
    if (complex_layout) {
        width = shapeRuns(utf8, [](std::size_t, uint32_t, int, int) {});
    } else {
        for (uint32_t codepoint : codepoints) {
            width += renderedGlyph(codepoint).advance;
        }
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

    // Shared by both paths so they cannot drift apart.
    auto blitGlyph = [&](const GlyphBitmap& glyph, int x_pos, int y_pos) {
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
    };

    if (complex_layout) {
        shapeRuns(utf8, [&](std::size_t face_index, uint32_t glyph_id, int x, int y) {
            const GlyphBitmap& glyph = shapedGlyph(face_index, glyph_id);
            if (glyph.valid) {
                blitGlyph(glyph, x + glyph.left, baseline - glyph.top + y);
            }
        });
        return sfc;
    }

    int pen_x = 0;
    for (uint32_t codepoint : codepoints) {
        const GlyphBitmap& glyph = renderedGlyph(codepoint);
        if (!glyph.valid) {
            continue;
        }

        const int y_pos = baseline - glyph.top;
        const int x_pos = pen_x + glyph.left;

        // Coverage is cached rather than finished pixels, so the blend against
        // the caller's colours happens here.
        blitGlyph(glyph, x_pos, y_pos);

        pen_x += glyph.advance;
    }
    return sfc;
}

bool Font::isValid()
{
    return m_face != nullptr;
}

} // namespace PsyMP3::Core
