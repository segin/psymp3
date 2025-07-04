/*
 * Widget.h - Extensible widget class.
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef WIDGET_H
#define WIDGET_H

class Widget : public Surface
{
    public:
        Widget();
        virtual ~Widget();
        // Widget objects are not copyable, as they contain a unique_ptr via Surface base.
        Widget(const Widget&) = delete;
        Widget& operator=(const Widget&) = delete;
        // Allow moving
        Widget(Widget&&) = default;
        Widget& operator=(Widget&&) = default;
        explicit Widget(Surface&& other); // Take ownership by moving a Surface
        Widget(Surface&& other, const Rect& position); // Take ownership by moving a Surface
        virtual void BlitTo(Surface& target);
        // Removed operator=(const Surface& rhs) as it's not copy-assignable.
        void setPos(const Rect& position);
        const Rect& getPos() const { return m_pos; }
        void setSurface(std::unique_ptr<Surface> surface);
        void addChild(std::unique_ptr<Widget> child);
        
        // Generic mouse event handling (propagates to children)
        virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
        virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y);
        virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
    protected:
        void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos);
        Surface& getSurface();
        Rect m_pos;
        std::vector<std::unique_ptr<Widget>> m_children;
        
        // Mouse capture system
        static Widget* s_mouse_captured_widget;
        void captureMouse();
        void releaseMouse();
        bool hasMouseCapture() const;
    private:
};

#endif // WIDGET_H
