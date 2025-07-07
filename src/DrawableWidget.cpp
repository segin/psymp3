/*
 * DrawableWidget.cpp - Implementation for custom drawing widget base class
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

DrawableWidget::DrawableWidget(int width, int height)
    : Widget()
    , m_needs_redraw(true)
{
    // Set widget size
    Rect widget_rect(0, 0, width, height);
    setPos(widget_rect);
    
    // Note: Don't call updateSurface() here as it calls pure virtual draw()
    // and derived class isn't fully constructed yet. updateSurface() will be
    // called automatically on first BlitTo() when m_needs_redraw is true.
}

void DrawableWidget::invalidate()
{
    m_needs_redraw = true;
}

void DrawableWidget::redraw()
{
    m_needs_redraw = true;
    updateSurface();
}

void DrawableWidget::onResize(int new_width, int new_height)
{
    // Update widget position
    Rect current_pos = getPos();
    setPos(Rect(current_pos.x(), current_pos.y(), new_width, new_height));
    
    // Mark for redraw since size changed
    invalidate();
    updateSurface();
}

void DrawableWidget::BlitTo(Surface& target)
{
    Debug::widgetBlit("DrawableWidget::BlitTo - updating surface first");
    
    // Ensure surface is up-to-date before blitting
    updateSurface();
    
    Debug::widgetBlit("DrawableWidget::BlitTo - calling parent Widget::BlitTo");
    
    // Call parent implementation to do the actual blitting
    Widget::BlitTo(target);
}

void DrawableWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    // Ensure surface is up-to-date before blitting
    updateSurface();
    
    // Call parent implementation to do the actual blitting with proper coordinates
    Widget::recursiveBlitTo(target, parent_absolute_pos);
}

void DrawableWidget::updateSurface()
{
    if (!m_needs_redraw) {
        return;
    }
    
    Rect pos = getPos();
    
    // Validate dimensions to prevent crashes
    if (pos.width() <= 0 || pos.height() <= 0 || pos.width() > 10000 || pos.height() > 10000) {
        return;
    }
    
    // Create new surface for drawing
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    
    // Call the subclass draw method
    draw(*surface);
    
    // Set the new surface
    setSurface(std::move(surface));
    
    m_needs_redraw = false;
}