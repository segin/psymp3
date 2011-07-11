/*
 * display.h - class header for SDL display wrapper
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

#ifndef DISPLAY_H
#define DISPLAY_H

class Display
{
    public:
        Display();
        virtual ~Display();
        bool isInit();
        void pixel(int16_t x, int16_t y, uint32_t color);
        void pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        /*
int rectangleColor(SDL_Surface * dst, Sint16 x1, Sint16 y1,
Sint16 x2, Sint16 y2, Uint32 color);
int rectangleRGBA(SDL_Surface * dst, Sint16 x1, Sint16 y1,
Sint16 x2, Sint16 y2, Uint8 r, Uint8 g,
Uint8 b, Uint8 a);
Filled rectangle (Box)
int boxColor(SDL_Surface * dst, Sint16 x1, Sint16 y1,
Sint16 x2, Sint16 y2, Uint32 color);
int boxRGBA(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2,
Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);


Horizontal line
int hlineColor(SDL_Surface * dst, Sint16 x1, Sint16 x2,
Sint16 y, Uint32 color);
int hlineRGBA(SDL_Surface * dst, Sint16 x1, Sint16 x2,
Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
Vertical line
int vlineColor(SDL_Surface * dst, Sint16 x, Sint16 y1,
Sint16 y2, Uint32 color);
int vlineRGBA(SDL_Surface * dst, Sint16 x, Sint16 y1,
Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
Rectangle
*/
        SDL_Surface * getHandle();

    protected:
    private:
        SDL_Surface * m_handle; //!< Member variable "m_handle"
};

#endif // DISPLAY_H