/*
 * font.cpp - wrapper for SDL_ttf's TTF_Font type, class implementation.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "psymp3.h"

Font::Font(TagLib::String file, int ptsize)
{
    if(!TTF_WasInit()) return;
    std::cout << "Font::Font(): Requesting font " << file.to8Bit(true) << std::endl;
    m_font = TTF_OpenFont(file.toCString(), ptsize);
    std::cout << "Font::Font(): m_font = " << std::hex << m_font << std::endl;
}

Font::~Font()
{
    //dtor
    TTF_CloseFont(m_font);
}

Surface* Font::Render(TagLib::String text, uint8_t r, uint8_t g, uint8_t b)
{
    if (!m_font) return new Surface();
    SDL_Color sdlcolor;
    sdlcolor.r = r;
    sdlcolor.g = g;
    sdlcolor.b = b;
    return new Surface(TTF_RenderUTF8_Blended(m_font, text.toCString(true), sdlcolor));
}

bool Font::isValid()
{
    if(m_font)
        return true;
    else
        return false;
}
