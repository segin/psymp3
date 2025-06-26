/*
 * display.h - SDL_Surface wrapper header for display window.
 * This also wraps SDL_gfx for primitives.
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

Display::Display() : Surface(SDL_SetVideoMode(640, 400, 16, SDL_HWSURFACE | SDL_DOUBLEBUF))
{
    //ctor
    std::cout << "Display::Display() got " << std::hex << getHandle() << std::endl;
    SDL_WM_SetCaption("PsyMP3 " PSYMP3_VERSION, "PsyMP3");
}

void Display::SetCaption(TagLib::String title, TagLib::String icon_title)
{
    SDL_WM_SetCaption(title.toCString(false), icon_title.toCString(false));
}
