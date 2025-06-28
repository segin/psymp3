/*
 * Label.cpp - A text label widget implementation.
 * This file is part of PsyMP3.
 * Copyright © 2024 Kirn Gill <segin2005@gmail.com>
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

Label::Label(Font* font, const Rect& position, const TagLib::String& initial_text, SDL_Color color)
    : Widget(Surface(), position), // Initialize base Widget with an empty surface and a position
      m_font(font),
      m_text(), // Will be set by setText
      m_color(color)
{
    // The initial render is done by calling setText.
    setText(initial_text);
}

void Label::setText(const TagLib::String& text)
{
    // Avoid re-rendering if the text hasn't changed.
    if (text == m_text) {
        return;
    }

    m_text = text;

    // Render the new text onto a temporary surface and then move-assign it to our base Surface.
    // This updates the widget's visual representation.
    *static_cast<Surface*>(this) = m_font->Render(m_text, m_color.r, m_color.g, m_color.b);
}