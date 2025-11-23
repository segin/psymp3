/*
 * Button.cpp - Implementation for generic reusable button widget
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

namespace PsyMP3 {
namespace Widget {
namespace UI {

using Foundation::Widget;
using Foundation::DrawableWidget;

ButtonWidget::ButtonWidget(int width, int height, ButtonSymbol symbol)
    : Widget()
    , m_symbol(symbol)
    , m_pressed(false)
    , m_hovered(false)
    , m_enabled(true)
    , m_global_mouse_tracking(false)
{
    // Set button size
    Rect button_rect(0, 0, width, height);
    setPos(button_rect);
    
    // Create initial surface
    rebuildSurface();
}

bool ButtonWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled || event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    
    // Check if click is within button bounds
    Rect pos = getPos();
    if (relative_x >= 0 && relative_x < pos.width() && 
        relative_y >= 0 && relative_y < pos.height()) {
        m_pressed = true;
        
        // Global mouse tracking enabled (SDL 1.2 doesn't have mouse capture)
        
        rebuildSurface();
        return true;
    }
    
    return false;
}

bool ButtonWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled || event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    
    if (m_pressed) {
        m_pressed = false;
        
        // Global mouse tracking disabled (SDL 1.2 doesn't have mouse capture)
        
        // Check if mouse is still over button for click
        Rect pos = getPos();
        bool mouse_over_button = (relative_x >= 0 && relative_x < pos.width() && 
                                 relative_y >= 0 && relative_y < pos.height());
        
        // For global tracking, always consider it a valid click if button was pressed
        if (mouse_over_button || m_global_mouse_tracking) {
            if (m_on_click) {
                m_on_click();
            }
        }
        
        rebuildSurface();
        return true;
    }
    
    return false;
}

bool ButtonWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (!m_enabled) {
        return false;
    }
    
    // Update hover state
    Rect pos = getPos();
    bool was_hovered = m_hovered;
    m_hovered = (relative_x >= 0 && relative_x < pos.width() && 
                relative_y >= 0 && relative_y < pos.height());
    
    if (was_hovered != m_hovered) {
        rebuildSurface();
    }
    
    return m_hovered;
}

void ButtonWidget::setSymbol(ButtonSymbol symbol)
{
    if (m_symbol != symbol) {
        m_symbol = symbol;
        rebuildSurface();
    }
}

void ButtonWidget::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!enabled) {
            m_pressed = false;
            m_hovered = false;
        }
        rebuildSurface();
    }
}

void ButtonWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    
    // Draw button background
    drawButtonBackground(*surface, m_pressed);
    
    // Draw button symbol
    drawButtonSymbol(*surface, m_symbol, m_enabled);
    
    setSurface(std::move(surface));
}

void ButtonWidget::drawButtonBackground(Surface& surface, bool pressed)
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    // Button background (light gray)
    surface.box(0, 0, width - 1, height - 1, 192, 192, 192, 255);
    
    if (pressed) {
        // Pressed button - inverted bevel (dark on top/left, light on bottom/right)
        // Top and left shadow (dark gray) - with 45-degree corner cut
        surface.hline(1, width - 2, 0, 128, 128, 128, 255); // Top line: start at (2,1) in one-indexed
        surface.vline(0, 1, height - 2, 128, 128, 128, 255); // Left line: start at (1,2) in one-indexed
        
        // Bottom and right highlight (white/light) - exact coordinates as specified
        // Outer highlight lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(0, width - 1, height - 1, 255, 255, 255, 255); // (1,18)-(18,18)
        surface.vline(width - 1, 0, height - 1, 255, 255, 255, 255); // (18,1)-(18,18)
        
        // Inner shadow lines: (2,2)-(17,2) and (2,2)-(2,17) for pressed state
        surface.hline(1, width - 2, 1, 128, 128, 128, 255); // (2,2)-(17,2)
        surface.vline(1, 1, height - 2, 128, 128, 128, 255); // (2,2)-(2,17)
    } else {
        // Normal button - standard 3D bevel (light on top/left, dark on bottom/right)
        // Top and left highlight (white/light) - covering the full corner
        surface.hline(0, width - 2, 0, 255, 255, 255, 255); // Top line: start at (1,1) in one-indexed
        surface.vline(0, 0, height - 2, 255, 255, 255, 255); // Left line: start at (1,1) in one-indexed
        
        // Bottom and right shadow (dark gray) - exact coordinates as specified
        // Outer shading lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(0, width - 1, height - 1, 128, 128, 128, 255); // (1,18)-(18,18)
        surface.vline(width - 1, 0, height - 1, 128, 128, 128, 255); // (18,1)-(18,18)
        
        // Inner shading lines: (2,17)-(17,17) and (17,2)-(17,17)
        surface.hline(1, width - 2, height - 2, 128, 128, 128, 255); // (2,17)-(17,17)
        surface.vline(width - 2, 1, height - 2, 128, 128, 128, 255); // (17,2)-(17,17)
    }
}

void ButtonWidget::drawButtonSymbol(Surface& surface, ButtonSymbol symbol, bool enabled)
{
    Rect pos = getPos();
    int center_x = pos.width() / 2;
    int center_y = pos.height() / 2;
    
    // Use black for enabled, gray for disabled
    uint8_t r = enabled ? 0 : 128;
    uint8_t g = enabled ? 0 : 128;
    uint8_t b = enabled ? 0 : 128;
    
    switch (symbol) {
        case ButtonSymbol::None:
            // No symbol to draw
            break;
            
        case ButtonSymbol::Minimize:
        case ButtonSymbol::ScrollDown:
            // Downward triangle
            {
                Sint16 x1 = center_x - 3;
                Sint16 y1 = center_y - 1;
                Sint16 x2 = center_x + 3;
                Sint16 y2 = center_y - 1;
                Sint16 x3 = center_x;
                Sint16 y3 = center_y + 2;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Maximize:
        case ButtonSymbol::ScrollUp:
            // Upward triangle
            {
                Sint16 x1 = center_x;
                Sint16 y1 = center_y - 2;
                Sint16 x2 = center_x + 3;
                Sint16 y2 = center_y + 1;
                Sint16 x3 = center_x - 3;
                Sint16 y3 = center_y + 1;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::ScrollLeft:
            // Leftward triangle
            {
                Sint16 x1 = center_x + 1;
                Sint16 y1 = center_y - 3;
                Sint16 x2 = center_x + 1;
                Sint16 y2 = center_y + 3;
                Sint16 x3 = center_x - 2;
                Sint16 y3 = center_y;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::ScrollRight:
            // Rightward triangle
            {
                Sint16 x1 = center_x - 1;
                Sint16 y1 = center_y - 3;
                Sint16 x2 = center_x - 1;
                Sint16 y2 = center_y + 3;
                Sint16 x3 = center_x + 2;
                Sint16 y3 = center_y;
                surface.filledTriangle(x1, y1, x2, y2, x3, y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Restore:
            // Two overlapping triangles
            {
                // Maximize triangle (bottom-left)
                Sint16 max_x1 = center_x - 3;
                Sint16 max_y1 = center_y - 1;
                Sint16 max_x2 = center_x;
                Sint16 max_y2 = center_y + 2;
                Sint16 max_x3 = center_x - 6;
                Sint16 max_y3 = center_y + 2;
                surface.filledTriangle(max_x1, max_y1, max_x2, max_y2, max_x3, max_y3, r, g, b, 255);
                
                // Minimize triangle (top-right)
                Sint16 min_x1 = center_x - 3;
                Sint16 min_y1 = center_y + 2;
                Sint16 min_x2 = center_x + 3;
                Sint16 min_y2 = center_y + 2;
                Sint16 min_x3 = center_x;
                Sint16 min_y3 = center_y + 5;
                surface.filledTriangle(min_x1, min_y1, min_x2, min_y2, min_x3, min_y3, r, g, b, 255);
            }
            break;
            
        case ButtonSymbol::Close:
            // X symbol (two diagonal lines)
            {
                int size = 3;
                for (int i = -size; i <= size; ++i) {
                    // Main diagonal
                    surface.pixel(center_x + i, center_y + i, r, g, b, 255);
                    // Anti-diagonal
                    surface.pixel(center_x + i, center_y - i, r, g, b, 255);
                }
            }
            break;
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
