/*
 * font.cpp - wrapper for SDL_ttf's TTF_Font type, class header.
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

#ifndef FONT_H
#define FONT_H

class Font
{
    public:
        explicit Font(TagLib::String file, int ptsize = 12);
        virtual ~Font();
        Surface* Render(TagLib::String text, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255);
        bool isValid();
    protected:
    private:
        TTF_Font * m_font;
};

#endif // FONT_H
