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

Font::Font(TagLib::String file, int ptsize)
{
    if(!TTF_WasInit()) {
        std::cerr << "Font::Font(): SDL_ttf was not initalized (did you call TrueType::Init()?)" << std::endl;
        return;
    }
    std::cout << "Font::Font(): Requesting font " << file.to8Bit(true) << std::endl;
    m_font = TTF_OpenFont(file.toCString(), ptsize);
    std::cout << "Font::Font(): m_font = " << std::hex << m_font << std::endl;
}

Font::~Font()
{
    //dtor
    TTF_CloseFont(m_font);
}

Surface Font::Render(TagLib::String text, uint8_t r, uint8_t g, uint8_t b)
{
    Surface surface; // Default-constructs an empty surface with the correct deleter
    if (!m_font) return surface;

    SDL_Color sdlcolor;
    sdlcolor.r = r;
    sdlcolor.g = g;
    sdlcolor.b = b;
    surface.m_handle.reset(TTF_RenderUTF8_Blended(m_font, text.toCString(true), sdlcolor));
    return surface;
}

bool Font::isValid()
{
    if(m_font)
        return true;
    else
        return false;
}
