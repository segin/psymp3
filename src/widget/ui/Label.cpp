/*
 * Label.cpp - A text label widget implementation.
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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

namespace PsyMP3 {
namespace Widget {
namespace UI {

Label::Label(Font* font, const Rect& position, const TagLib::String& initial_text, SDL_Color color, SDL_Color background_color)
    : Widget(Surface(), position), // Initialize base Widget with an empty surface and a position
      m_font(font),
      m_text(), // Will be set by setText
      m_color(color),
      m_background_color(background_color)
{
    // The initial render is done by calling setText.
    setText(initial_text);
}

void Label::setBackgroundColor(SDL_Color background_color)
{
    m_background_color = background_color;
    invalidate();
}

void Label::setText(const TagLib::String& text)
{
    // Avoid re-rendering if the text hasn't changed.
    if (text == m_text) {
        return;
    }

    m_text = text;

    m_text_surface = m_font->Render(m_text, m_color.r, m_color.g, m_color.b);
    if (!m_text_surface) {
        std::cerr << "Failed to render text surface for label." << std::endl;
        return;
    }
    // Enable per-pixel alpha for proper transparent text rendering
    m_text_surface->SetAlpha(SDL_SRCALPHA | SDL_RLEACCEL, SDL_ALPHA_TRANSPARENT);
    setSurface(std::move(m_text_surface));
    
    // Notify parent that this widget needs repainting
    invalidate();
}

void Label::BlitTo(Surface& target)
{
    blitWithBackgroundClear(target, m_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, m_pos);
    }
}

void Label::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    blitWithBackgroundClear(target, absolute_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, absolute_pos);
    }
}

void Label::blitWithBackgroundClear(Surface& target, const Rect& absolute_pos)
{
    if (!this->isValid()) {
        return;
    }

    // Labels render onto a persistent graph surface, so the old glyph bounds
    // must be cleared before alpha-blending the newly rendered text.
    int clear_w = std::max({static_cast<int>(absolute_pos.width()), m_last_drawn_width,
                            m_text_surface ? static_cast<int>(m_text_surface->width()) : 0});
    int clear_h = std::max({static_cast<int>(absolute_pos.height()), m_last_drawn_height,
                            m_text_surface ? static_cast<int>(m_text_surface->height()) : 0});

    target.box(absolute_pos.x(), absolute_pos.y(),
               absolute_pos.x() + clear_w - 1,
               absolute_pos.y() + clear_h - 1,
               target.MapRGB(m_background_color.r, m_background_color.g, m_background_color.b));
    target.Blit(*this, absolute_pos);

    m_last_drawn_width = m_text_surface ? m_text_surface->width() : 0;
    m_last_drawn_height = m_text_surface ? m_text_surface->height() : 0;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
