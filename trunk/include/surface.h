/*
 * display.h - class implementation for SDL display wrapper
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

#ifndef SURFACE_H
#define SURFACE_H

class Surface
{
    public:
        Surface();
        Surface(SDL_Surface *sfc);
        virtual ~Surface();
        bool isInit();
        void pixel(int16_t x, int16_t y, uint32_t color);
        void pixel(int16_t x, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void rectangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint32_t color);
        void box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void hline(int16_t x1, int16_t x2, int16_t y, uint32_t color);
        void hline(int16_t x1, int16_t x2, int16_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        void vline(int16_t x, int16_t y1, int16_t y2, uint32_t color);
        void vline(int16_t x, int16_t y1, int16_t y2, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
        SDL_Surface * getHandle();
    protected:
        SDL_Surface * m_handle; //!< Member variable "m_handle"
    private:

};


#endif // SURFACE_H
