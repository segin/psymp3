/*
 * rect.h - Rect class header
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

#ifndef RECT_H
#define RECT_H

class Rect
{
    public:
        Rect();
        Rect(int16_t x, int16_t y, uint16_t w, uint16_t h); // New constructor for full rect
        Rect(uint16_t w, uint16_t h); // Existing constructor, now for width/height only (assumes x=0, y=0)
        ~Rect();
        int16_t x() const { return m_x; };
        int16_t y() const { return m_y; };
        uint16_t width() const { return m_width; };
        uint16_t height() const { return m_height; };
        void x(int16_t val) { m_x = val; };
        void y(int16_t val) { m_y = val; };
        void width(uint16_t a) { m_width = a; };
        void height(uint16_t a) { m_height = a; };
    protected:
    private:
        int16_t m_x;
        int16_t m_y;
        uint16_t m_width;
        uint16_t m_height;
};

#endif // RECT_H
