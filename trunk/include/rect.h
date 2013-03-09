/*
 * rect.h - Rect class header
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

#ifndef RECT_H
#define RECT_H

class Rect
{
    public:
        Rect();
        Rect(unsigned int, unsigned int);
        ~Rect();
        unsigned int width() { return m_width; };
        unsigned int height() { return m_height; };
        void width(unsigned int a) { m_width = a; };
        void height(unsigned int a) { m_height = a; };
    protected:
    private:
        unsigned int m_width;
        unsigned int m_height;
};

#endif // RECT_H
