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

#include <utility> // For std::pair
#include <cstdint> // For int16_t and uint16_t
#include <string>  // For std::string

class Rect
{
    public:
        Rect();
        Rect(int16_t x, int16_t y, uint16_t w, uint16_t h); // New constructor for full rect
        Rect(uint16_t w, uint16_t h); // Existing constructor, now for width/height only (assumes x=0, y=0)
        ~Rect();
        // Accessor methods
        int16_t x() const;
        int16_t y() const;
        uint16_t width() const;
        uint16_t height() const;
        
        // Mutator methods
        void x(int16_t val);
        void y(int16_t val);
        void width(uint16_t a);
        void height(uint16_t a);
        
        // Edge coordinate methods
        int16_t left() const;
        int16_t top() const;
        int16_t right() const;
        int16_t bottom() const;
        
        // Center point calculation methods
        int16_t centerX() const;
        int16_t centerY() const;
        std::pair<int16_t, int16_t> center() const;
        
        // Area and validation methods
        uint32_t area() const;
        bool isEmpty() const;
        bool isValid() const;
        
        // Geometric operation methods
        bool contains(int16_t x, int16_t y) const;
        bool contains(const Rect& other) const;
        bool intersects(const Rect& other) const;
        Rect intersection(const Rect& other) const;
        Rect united(const Rect& other) const;
        
        // Expansion and contraction methods
        void expand(int16_t margin);
        void expand(int16_t dx, int16_t dy);
        Rect expanded(int16_t margin) const;
        Rect expanded(int16_t dx, int16_t dy) const;
        void shrink(int16_t margin);
        void shrink(int16_t dx, int16_t dy);
        Rect shrunk(int16_t margin) const;
        Rect shrunk(int16_t dx, int16_t dy) const;
        
        // Translation methods
        void translate(int16_t dx, int16_t dy);
        Rect translated(int16_t dx, int16_t dy) const;
        void moveTo(int16_t x, int16_t y);
        Rect movedTo(int16_t x, int16_t y) const;
        
        // Resizing methods
        void resize(uint16_t width, uint16_t height);
        Rect resized(uint16_t width, uint16_t height) const;
        
        // Combined adjustment methods
        void adjust(int16_t dx, int16_t dy, int16_t dw, int16_t dh);
        Rect adjusted(int16_t dx, int16_t dy, int16_t dw, int16_t dh) const;
        
        // Centering operations
        void centerIn(const Rect& container);
        Rect centeredIn(const Rect& container) const;
        
        // Comparison operators
        bool operator==(const Rect& other) const;
        bool operator!=(const Rect& other) const;
        
        // String representation and debugging support
        std::string toString() const;
    protected:
    private:
        int16_t m_x;
        int16_t m_y;
        uint16_t m_width;
        uint16_t m_height;
};

#endif // RECT_H
