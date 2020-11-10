/*
 * Widget.cpp - Extensible widget class, extends Surface.
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

Widget::Widget()
{
    //ctor
}

Widget::~Widget()
{
    //dtor
}

Widget::Widget(const Surface& other) :
    Surface(other),
    m_pos(0, 0)
{
}

Widget::Widget(const Widget& other)
{
}

Widget::Widget(const Surface& other, Rect& position) :
    Surface(other),
    m_pos(position)
{
}

void Widget::BlitTo(Surface& target)
{
    target.Blit(*this, m_pos);
}

void Widget::operator=(const Surface& rhs)
{
    m_handle = rhs.m_handle;
}

void Widget::updatePosition(const Rect& position)
{
    m_pos = position;
}
