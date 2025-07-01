/*
 * Label.cpp - A text label widget implementation.
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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
    std::cout << "Label constructor called." << std::endl;
    // The initial render is done by calling setText.
    setText(initial_text);
    std::cout << "Label constructor finished." << std::endl;
}

void Label::setText(const TagLib::String& text)
{
    std::cout << "Label::setText called with text: " << text.to8Bit(true) << std::endl;
    // Avoid re-rendering if the text hasn't changed.
    if (text == m_text) {
        std::cout << "Text unchanged, skipping render." << std::endl;
        return;
    }

    m_text = text;

    // Render the new text onto a temporary surface and then move-assign it to our base Surface.
    // This updates the widget's visual representation.
    m_text_surface = m_font->Render(m_text, m_color.r, m_color.g, m_color.b);
    if (!m_text_surface) {
        std::cerr << "Failed to render text surface for label." << std::endl;
        return;
    }
    std::cout << "Text surface rendered successfully." << std::endl;
    setSurface(std::move(m_text_surface));
    std::cout << "Label::setText finished." << std::endl;
}