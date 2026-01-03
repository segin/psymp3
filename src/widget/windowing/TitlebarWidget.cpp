/*
 * TitlebarWidget.cpp - Implementation for window titlebar widget
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

using Foundation::Widget;

TitlebarWidget::TitlebarWidget(int width, int height, const std::string& title)
    : Widget()
    , m_title(title)
    , m_width(width)
    , m_height(height)
    , m_is_dragging(false)
    , m_last_mouse_x(0)
    , m_last_mouse_y(0)
{
    // Set initial position and size
    Rect pos(0, 0, width, height);
    setPos(pos);
    
    // Build the initial titlebar surface
    rebuildSurface();
}

void TitlebarWidget::setTitle(const std::string& title)
{
    if (m_title != title) {
        m_title = title;
        rebuildSurface();
    }
}

bool TitlebarWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        m_is_dragging = true;
        m_last_mouse_x = event.x;
        m_last_mouse_y = event.y;
        
        if (m_on_drag_start) {
            m_on_drag_start(event.x, event.y);
        }
        
        return true;
    }
    return false;
}

bool TitlebarWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (m_is_dragging) {
        int dx = event.x - m_last_mouse_x;
        int dy = event.y - m_last_mouse_y;
        
        m_last_mouse_x = event.x;
        m_last_mouse_y = event.y;
        
        if (m_on_drag) {
            m_on_drag(dx, dy);
        }
        
        return true;
    }
    return false;
}

bool TitlebarWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT && m_is_dragging) {
        m_is_dragging = false;
        
        if (m_on_drag_end) {
            m_on_drag_end();
        }
        
        return true;
    }
    return false;
}

void TitlebarWidget::rebuildSurface()
{
    // Create the titlebar surface
    auto titlebar_surface = std::make_unique<Surface>(m_width, m_height, true);
    
    // Fill with titlebar color (blue)
    uint32_t blue_color = titlebar_surface->MapRGB(64, 128, 255);
    titlebar_surface->FillRect(blue_color);
    
    // Add a border effect (darker gray outline)
    titlebar_surface->box(0, 0, m_width - 1, m_height - 1, 64, 64, 64, 255);
    
    // TODO: Add title text rendering when font access is available
    // For now, the titlebar is just a solid colored bar
    
    // Set the surface
    setSurface(std::move(titlebar_surface));
}

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3
