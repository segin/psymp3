/*
 * ApplicationWidget.cpp - Implementation for root application widget
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

// Static instance for singleton
std::unique_ptr<ApplicationWidget> ApplicationWidget::s_instance = nullptr;

ApplicationWidget& ApplicationWidget::getInstance(Display& display)
{
    if (!s_instance) {
        s_instance = std::unique_ptr<ApplicationWidget>(new ApplicationWidget(display));
    }
    return *s_instance;
}

ApplicationWidget& ApplicationWidget::getInstance()
{
    if (!s_instance) {
        throw std::runtime_error("ApplicationWidget not initialized. Call getInstance(Display&) first.");
    }
    return *s_instance;
}

ApplicationWidget::ApplicationWidget(Display& display)
    : Widget()
    , m_display(display)
{
    // Set position to cover entire screen
    Rect screen_rect(0, 0, display.width(), display.height());
    setPos(screen_rect);
    
    // Create a surface for the entire application
    rebuildSurface();
}

bool ApplicationWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // First check if any window should handle this event (top to bottom)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        const auto& window = *it;
        Rect window_pos = window->getPos();
        
        // Check if mouse is within window bounds
        if (relative_x >= window_pos.x() && relative_x < window_pos.x() + window_pos.width() &&
            relative_y >= window_pos.y() && relative_y < window_pos.y() + window_pos.height()) {
            
            // Calculate relative coordinates for window
            int window_relative_x = relative_x - window_pos.x();
            int window_relative_y = relative_y - window_pos.y();
            
            // Bring window to front if clicked
            bringWindowToFront(window.get());
            
            // Forward event to window
            if (window->handleMouseDown(event, window_relative_x, window_relative_y)) {
                return true; // Event was handled by window
            }
        }
    }
    
    // If no window handled it, delegate to child widgets (desktop elements)
    return Widget::handleMouseDown(event, relative_x, relative_y);
}

bool ApplicationWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    // Handle mouse capture first
    if (s_mouse_captured_widget) {
        // Find the captured widget and forward the event directly
        for (const auto& window : m_windows) {
            if (window.get() == s_mouse_captured_widget) {
                Rect window_pos = window->getPos();
                int window_relative_x = relative_x - window_pos.x();
                int window_relative_y = relative_y - window_pos.y();
                return window->handleMouseMotion(event, window_relative_x, window_relative_y);
            }
        }
        
        // Check if captured widget is in our child widgets (desktop elements)
        return Widget::handleMouseMotion(event, relative_x, relative_y);
    }
    
    // Normal processing - check windows first (top to bottom)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        const auto& window = *it;
        Rect window_pos = window->getPos();
        
        // Check if mouse is within window bounds
        if (relative_x >= window_pos.x() && relative_x < window_pos.x() + window_pos.width() &&
            relative_y >= window_pos.y() && relative_y < window_pos.y() + window_pos.height()) {
            
            // Calculate relative coordinates for window
            int window_relative_x = relative_x - window_pos.x();
            int window_relative_y = relative_y - window_pos.y();
            
            // Forward event to window
            if (window->handleMouseMotion(event, window_relative_x, window_relative_y)) {
                return true; // Event was handled by window
            }
        }
    }
    
    // If no window handled it, delegate to child widgets (desktop elements)
    return Widget::handleMouseMotion(event, relative_x, relative_y);
}

bool ApplicationWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Handle mouse capture first
    if (s_mouse_captured_widget) {
        // Find the captured widget and forward the event directly
        for (const auto& window : m_windows) {
            if (window.get() == s_mouse_captured_widget) {
                Rect window_pos = window->getPos();
                int window_relative_x = relative_x - window_pos.x();
                int window_relative_y = relative_y - window_pos.y();
                return window->handleMouseUp(event, window_relative_x, window_relative_y);
            }
        }
        
        // Check if captured widget is in our child widgets (desktop elements)
        return Widget::handleMouseUp(event, relative_x, relative_y);
    }
    
    // Normal processing - check windows first (top to bottom)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        const auto& window = *it;
        Rect window_pos = window->getPos();
        
        // Check if mouse is within window bounds
        if (relative_x >= window_pos.x() && relative_x < window_pos.x() + window_pos.width() &&
            relative_y >= window_pos.y() && relative_y < window_pos.y() + window_pos.height()) {
            
            // Calculate relative coordinates for window
            int window_relative_x = relative_x - window_pos.x();
            int window_relative_y = relative_y - window_pos.y();
            
            // Forward event to window
            if (window->handleMouseUp(event, window_relative_x, window_relative_y)) {
                return true; // Event was handled by window
            }
        }
    }
    
    // If no window handled it, delegate to child widgets (desktop elements)
    return Widget::handleMouseUp(event, relative_x, relative_y);
}

void ApplicationWidget::addWindow(std::unique_ptr<Widget> window)
{
    if (window) {
        m_windows.push_back(std::move(window));
        rebuildSurface();
    }
}

void ApplicationWidget::removeWindow(Widget* window)
{
    auto it = std::find_if(m_windows.begin(), m_windows.end(),
                          [window](const std::unique_ptr<Widget>& w) { return w.get() == window; });
    
    if (it != m_windows.end()) {
        m_windows.erase(it);
        rebuildSurface();
    }
}


void ApplicationWidget::bringWindowToFront(Widget* window)
{
    auto it = std::find_if(m_windows.begin(), m_windows.end(),
                          [window](const std::unique_ptr<Widget>& w) { return w.get() == window; });
    
    if (it != m_windows.end()) {
        // Move the window to the end of the vector (top of z-order)
        auto window_ptr = std::move(*it);
        m_windows.erase(it);
        m_windows.push_back(std::move(window_ptr));
        rebuildSurface();
    }
}

Widget* ApplicationWidget::findWindowAt(int x, int y) const
{
    // Check windows from top to bottom
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        const auto& window = *it;
        Rect window_pos = window->getPos();
        
        if (x >= window_pos.x() && x < window_pos.x() + window_pos.width() &&
            y >= window_pos.y() && y < window_pos.y() + window_pos.height()) {
            return window.get();
        }
    }
    
    return nullptr;
}

void ApplicationWidget::rebuildSurface()
{
    Rect screen_rect = getPos();
    auto surface = std::make_unique<Surface>(screen_rect.width(), screen_rect.height(), true);
    
    // Fill with default background color (could be customizable)
    uint32_t bg_color = surface->MapRGB(64, 64, 64); // Dark gray background
    surface->FillRect(bg_color);
    
    // Render child widgets (desktop elements) in z-order (bottom to top)
    for (const auto& child : m_children) {
        if (child->isValid()) {
            Rect child_pos = child->getPos();
            surface->Blit(*child, child_pos);
        }
    }
    
    // Render all windows on top of desktop elements
    for (const auto& window : m_windows) {
        if (window->isValid()) {
            Rect window_pos = window->getPos();
            surface->Blit(*window, window_pos);
        }
    }
    
    setSurface(std::move(surface));
}