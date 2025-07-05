/*
 * LayoutWidget.cpp - Generic container widget for layout and grouping
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

LayoutWidget::LayoutWidget(int width, int height, bool transparent)
    : Widget()
    , m_transparent(transparent)
    , m_bg_r(0), m_bg_g(0), m_bg_b(0), m_bg_a(255)
{
    // Set initial size
    setPos(Rect(0, 0, width, height));
    
    // Create initial background if size is specified
    if (width > 0 && height > 0) {
        updateBackground();
    }
}

void LayoutWidget::setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    m_transparent = false;
    m_bg_r = r;
    m_bg_g = g;
    m_bg_b = b;
    m_bg_a = a;
    updateBackground();
}

void LayoutWidget::setTransparent()
{
    m_transparent = true;
    updateBackground();
}

void LayoutWidget::resize(int new_width, int new_height)
{
    Rect current_pos = getPos();
    setPos(Rect(current_pos.x(), current_pos.y(), new_width, new_height));
    updateBackground();
}

void LayoutWidget::updateBackground()
{
    Rect pos = getPos();
    
    // Don't create surface if no size is set
    if (pos.width() <= 0 || pos.height() <= 0) {
        return;
    }
    
    // Create background surface
    auto background = std::make_unique<Surface>(pos.width(), pos.height(), true);
    
    if (m_transparent) {
        // Fill with transparent background
        uint32_t transparent = background->MapRGBA(0, 0, 0, 0);
        background->FillRect(transparent);
    } else {
        // Fill with specified background color
        uint32_t bg_color = background->MapRGBA(m_bg_r, m_bg_g, m_bg_b, m_bg_a);
        background->FillRect(bg_color);
    }
    
    setSurface(std::move(background));
}