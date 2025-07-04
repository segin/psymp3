/*
 * WindowFrameWidget.cpp - Implementation for classic window frame widget
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

int WindowFrameWidget::s_next_z_order = 1;

WindowFrameWidget::WindowFrameWidget(int client_width, int client_height, const std::string& title)
    : Widget()
    , m_title(title)
    , m_client_width(client_width)
    , m_client_height(client_height)
    , m_z_order(s_next_z_order++)
    , m_is_dragging(false)
    , m_last_mouse_x(0)
    , m_last_mouse_y(0)
{
    // Calculate total window size (client area + decorations)
    int total_width = client_width + (BORDER_WIDTH * 2);
    int total_height = client_height + TITLEBAR_HEIGHT + (BORDER_WIDTH * 2);
    
    // Set initial position and size
    Rect pos(100, 100, total_width, total_height);
    setPos(pos);
    
    // Create default client area
    auto client_area = createDefaultClientArea();
    m_client_area = client_area.get();
    
    // Add client area as child widget
    addChild(std::move(client_area));
    
    // Update layout
    updateLayout();
    
    // Create window frame surface
    rebuildSurface();
}

bool WindowFrameWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        // Check if click is in titlebar for dragging
        if (isInTitlebar(relative_x, relative_y)) {
            m_is_dragging = true;
            m_last_mouse_x = event.x;
            m_last_mouse_y = event.y;
            
            if (m_on_drag_start) {
                m_on_drag_start();
            }
            
            return true;
        }
        
        // Otherwise, bring window to front and forward to client area
        bringToFront();
        
        // Check if click is in client area
        if (m_client_area) {
            Rect client_pos = m_client_area->getPos();
            if (relative_x >= client_pos.x() && relative_x < client_pos.x() + client_pos.width() &&
                relative_y >= client_pos.y() && relative_y < client_pos.y() + client_pos.height()) {
                
                // Forward to client area (if it has mouse handling)
                return true;
            }
        }
    }
    
    return false;
}

bool WindowFrameWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
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

bool WindowFrameWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT && m_is_dragging) {
        m_is_dragging = false;
        return true;
    }
    
    return false;
}

void WindowFrameWidget::setTitle(const std::string& title)
{
    if (m_title != title) {
        m_title = title;
        rebuildSurface();
    }
}

void WindowFrameWidget::setClientArea(std::unique_ptr<Widget> client_widget)
{
    // Remove existing client area from children
    m_children.clear();
    
    // Add new client area
    m_client_area = client_widget.get();
    addChild(std::move(client_widget));
    
    // Update layout
    updateLayout();
}

void WindowFrameWidget::bringToFront()
{
    m_z_order = s_next_z_order++;
}

std::unique_ptr<Widget> WindowFrameWidget::createDefaultClientArea()
{
    auto client_widget = std::make_unique<Widget>();
    
    // Create white client area surface
    auto client_surface = std::make_unique<Surface>(m_client_width, m_client_height, true);
    uint32_t white_color = client_surface->MapRGB(255, 255, 255);
    client_surface->FillRect(white_color);
    
    client_widget->setSurface(std::move(client_surface));
    
    // Set client area size
    Rect client_rect(0, 0, m_client_width, m_client_height);
    client_widget->setPos(client_rect);
    
    return client_widget;
}

void WindowFrameWidget::rebuildSurface()
{
    // Calculate total window size
    int total_width = m_client_width + (BORDER_WIDTH * 2);
    int total_height = m_client_height + TITLEBAR_HEIGHT + (BORDER_WIDTH * 2);
    
    // Create the window frame surface
    auto frame_surface = std::make_unique<Surface>(total_width, total_height, true);
    
    // Fill with window background color (light gray)
    uint32_t bg_color = frame_surface->MapRGB(192, 192, 192);
    frame_surface->FillRect(bg_color);
    
    // Draw outer border (dark gray)
    frame_surface->rectangle(0, 0, total_width - 1, total_height - 1, 64, 64, 64, 255);
    
    // Draw inner border (white highlight)
    frame_surface->rectangle(1, 1, total_width - 2, total_height - 2, 255, 255, 255, 255);
    
    // Draw titlebar area (blue)
    frame_surface->box(BORDER_WIDTH, BORDER_WIDTH, 
                      total_width - BORDER_WIDTH - 1, BORDER_WIDTH + TITLEBAR_HEIGHT - 1, 
                      64, 128, 255, 255);
    
    // Draw titlebar border
    frame_surface->rectangle(BORDER_WIDTH, BORDER_WIDTH, 
                           total_width - BORDER_WIDTH - 1, BORDER_WIDTH + TITLEBAR_HEIGHT - 1, 
                           32, 64, 128, 255);
    
    // TODO: Add title text rendering when font access is available
    
    // Set the surface
    setSurface(std::move(frame_surface));
}

void WindowFrameWidget::updateLayout()
{
    // Position client area within the frame
    if (m_client_area) {
        Rect client_pos(BORDER_WIDTH,                    // X: after left border
                       BORDER_WIDTH + TITLEBAR_HEIGHT,  // Y: after top border and titlebar
                       m_client_width,                   // Width: client area width
                       m_client_height);                 // Height: client area height
        m_client_area->setPos(client_pos);
    }
}

bool WindowFrameWidget::isInTitlebar(int x, int y) const
{
    return (x >= BORDER_WIDTH && 
            x < m_client_width + BORDER_WIDTH &&
            y >= BORDER_WIDTH && 
            y < BORDER_WIDTH + TITLEBAR_HEIGHT);
}