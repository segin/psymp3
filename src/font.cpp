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

namespace {

void ensureTTFInitialized()
{
    static std::once_flag ttf_init_once;
    static std::exception_ptr init_error;

    std::call_once(ttf_init_once, []() {
        if (TTF_Init() != 0) {
            init_error = std::make_exception_ptr(
                std::runtime_error(std::string("Failed to initialize SDL2_ttf: ") + TTF_GetError()));
        }
    });

    if (init_error) {
        std::rethrow_exception(init_error);
    }
}

} // namespace

Font::Font(const TagLib::String& file, int ptsize)
{
    ensureTTFInitialized();

    Debug::log("font", "Loading SDL2_ttf font: ", file.to8Bit(true), ", ptsize: ", ptsize);
    m_font = TTF_OpenFont(file.toCString(), ptsize);
    if (!m_font) {
        throw std::runtime_error(std::string("Failed to load font: ") + file.to8Bit(true)
                                 + " (" + TTF_GetError() + ")");
    }
}

Font::~Font()
{
    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
}

std::unique_ptr<Surface> Font::Render(const TagLib::String& text, uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_font) {
        Debug::log("font", "Font::Render called with invalid SDL2_ttf font");
        return nullptr;
    }

    std::string text_utf8 = text.to8Bit(true);
    if (text_utf8.empty()) {
        return std::make_unique<Surface>(1, 1, true);
    }

    SDL_Color color{r, g, b, 255};
    SDL_Surface* rendered = TTF_RenderUTF8_Blended(m_font, text_utf8.c_str(), color);
    if (!rendered) {
        Debug::log("font", "TTF_RenderUTF8_Blended failed for text '", text_utf8, "': ", TTF_GetError());
        return nullptr;
    }

    SDL_SetSurfaceBlendMode(rendered, SDL_BLENDMODE_BLEND);

    auto surface = std::make_unique<Surface>(rendered->w, rendered->h, true);
    surface->FillRect(surface->MapRGBA(0, 0, 0, 0));

    Surface temp(rendered);
    surface->Blit(temp, Rect(0, 0, rendered->w, rendered->h));
    SDL_FreeSurface(rendered);

    return surface;
}

bool Font::isValid() const
{
    return m_font != nullptr;
}
