/*
 * display.h - class header for SDL display wrapper
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
        void pixel(int16_t x, int16_t y, uint32_t color);
        void pixel(int16_t x, int16_t y, int8_t r, int8_t g, int8_t b, int8_t a);
        SDL_Surface * getHandle();
    protected:
    private:
        SDL_Surface * m_handle; //!< Member variable "m_handle"
};

#endif // DISPLAY_H
