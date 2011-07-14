/*
 * display.h - SDL_Surface wrapper header for display window.
 * This also wraps SDL_gfx for primitives.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "psymp3.h"

Display::Display()
{
    //ctor
    m_handle = SDL_SetVideoMode(640, 400, 16, SDL_HWSURFACE | SDL_DOUBLEBUF);
    std::cout << "Display::Display() got 0x" << std::hex << (unsigned int) m_handle << std::endl;
    SDL_WM_SetCaption("PsyMP3 " PSYMP3_VERSION, "PsyMP3");
}

void Display::SetCaption(TagLib::String title, TagLib::String icon_title)
{
    SDL_WM_SetCaption(title.toCString(true), icon_title.toCString(true));
}
