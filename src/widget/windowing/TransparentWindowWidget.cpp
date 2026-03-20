/*
 * TransparentWindowWidget.cpp - Implementation for transparent floating window
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
namespace Windowing {

using Foundation::DrawableWidget;

TransparentWindowWidget::TransparentWindowWidget(int width, int height, float opacity, bool mouse_transparent)
    : DrawableWidget(width, height)
    , m_z_order(ZOrder::NORMAL)
    , m_opacity(opacity)
    , m_mouse_pass_through(mouse_transparent)
    , m_corner_radius(0)
    , m_bg_r(0), m_bg_g(0), m_bg_b(0)  // Default to black background
{
    // Clamp opacity to valid range
    m_opacity = std::max(0.0f, std::min(1.0f, m_opacity));
    
    // Set mouse transparency on the Widget base class as well
    setMouseTransparent(m_mouse_pass_through);
}

void TransparentWindowWidget::setOpacity(float opacity)
{
    float new_opacity = std::max(0.0f, std::min(1.0f, opacity));
    if (m_opacity != new_opacity) {
        m_opacity = new_opacity;
        invalidate(); // Trigger redraw
    }
}

void TransparentWindowWidget::setBackgroundColor(uint8_t r, uint8_t g, uint8_t b)
{
    if (m_bg_r != r || m_bg_g != g || m_bg_b != b) {
        m_bg_r = r;
        m_bg_g = g;
        m_bg_b = b;
        invalidate(); // Trigger redraw
    }
}

bool TransparentWindowWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (m_mouse_pass_through) {
        // Don't handle the event, let it pass through to widgets below
        return false;
    }
    
    // Handle normally (delegate to child widgets)
    return DrawableWidget::handleMouseDown(event, relative_x, relative_y);
}

bool TransparentWindowWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (m_mouse_pass_through) {
        // Don't handle the event, let it pass through to widgets below
        return false;
    }
    
    // Handle normally (delegate to child widgets)
    return DrawableWidget::handleMouseMotion(event, relative_x, relative_y);
}

bool TransparentWindowWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (m_mouse_pass_through) {
        // Don't handle the event, let it pass through to widgets below
        return false;
    }
    
    // Handle normally (delegate to child widgets)
    return DrawableWidget::handleMouseUp(event, relative_x, relative_y);
}

void TransparentWindowWidget::draw(Surface& surface)
{
    // Calculate alpha value from opacity
    uint8_t alpha = static_cast<uint8_t>(m_opacity * 255);
    
    // Create background color with alpha
    uint32_t bg_color = surface.MapRGBA(m_bg_r, m_bg_g, m_bg_b, alpha);
    
    if (m_corner_radius > 0) {
        // Draw rounded rectangle background
        drawRoundedRect(surface, 0, 0, surface.width(), surface.height(), m_corner_radius, bg_color);
    } else {
        // Draw simple rectangle background
        surface.FillRect(bg_color);
    }
}

void TransparentWindowWidget::drawRoundedRect(Surface& surface, int x, int y, int width, int height, 
                                             int radius, uint32_t color)
{
    if (radius <= 0) {
        // Radius too small, draw normal rectangle
        surface.FillRect(color);
        return;
    }

    int16_t x1 = static_cast<int16_t>(x);
    int16_t y1 = static_cast<int16_t>(y);
    int16_t x2 = static_cast<int16_t>(x + width - 1);
    int16_t y2 = static_cast<int16_t>(y + height - 1);

    surface.roundedBox(x1, y1, x2, y2, static_cast<int16_t>(radius), color);
}

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3
