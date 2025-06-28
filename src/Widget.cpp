/*
 * Widget.cpp - Extensible widget class, extends Surface.
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

#include "psymp3.h"

Widget::Widget()
{
    //ctor
}

Widget::~Widget()
{
    //dtor
}

// Move constructor: takes ownership of the Surface's internal SDL_Surface*
Widget::Widget(Surface&& other) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(0, 0)
{
}

// Move constructor with position: takes ownership of the Surface and sets position
Widget::Widget(Surface&& other, const Rect& position) :
    Surface(std::move(other)), // Move the base Surface part
    m_pos(position)
{
}

void Widget::BlitTo(Surface& target)
{
    // Blit this widget's own surface content first (if it has any)
    if (this->isValid()) {
        target.Blit(*this, m_pos);
    }

    // Then, recursively blit all children, passing this widget's position as the parent offset.
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, m_pos);
    }
}

void Widget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    // Calculate the child's absolute on-screen position by adding its relative position to the parent's absolute position.
    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    // Blit the child's own surface content.
    if (this->isValid()) {
        target.Blit(*this, absolute_pos);
    }

    // Recursively blit this child's children.
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, absolute_pos);
    }
}

void Widget::setPos(const Rect& position)
{
    m_pos = position;
}

void Widget::addChild(std::unique_ptr<Widget> child)
{
    m_children.push_back(std::move(child));
}
