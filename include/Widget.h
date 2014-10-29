/*
 * Widget.h - Extensible widget class.
 * This file is part of PsyMP3.
 * Copyright © 2014 Kirn Gill <segin2005@gmail.com>
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

#ifndef WIDGET_H
#define WIDGET_H

class Widget : public Surface
{
    public:
        Widget();
        virtual ~Widget();
        Widget(const Surface& other);
        Widget(const Widget& other);
        Widget(const Surface& other, Rect& position);
        void BlitTo(Surface& target);
        void operator=(const Surface& rhs);
        void updatePosition(const Rect& position);
    protected:
    private:
        Rect m_pos;
};

#endif // WIDGET_H
