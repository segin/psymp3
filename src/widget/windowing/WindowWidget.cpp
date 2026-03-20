/*
 * WindowWidget.cpp - Implementation for modular floating window widget
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

int WindowWidget::s_next_z_order = 1;

WindowWidget::WindowWidget(int client_width, int client_height, const std::string& title, Font* font)
    : Widget()
    , m_client_width(client_width)
    , m_client_height(client_height)
    , m_z_order(s_next_z_order++)
{
    // Create WindowFrameWidget that will handle all the system decorations
    m_frame_widget_owned = std::make_unique<WindowFrameWidget>(client_width, client_height, title, font);
    m_frame_widget = m_frame_widget_owned.get();
    
    // Set up frame widget callbacks to forward events to our event handlers
    m_frame_widget->setOnDrag([this](int dx, int dy) {
        // Move the entire window
        Rect current_pos = getPos();
        current_pos.x(current_pos.x() + dx);
        current_pos.y(current_pos.y() + dy);
        setPos(current_pos);
        
        // Trigger drag event
        WindowEventData drag_event{};
        drag_event.type = WindowEvent::DRAG_MOVE;
        drag_event.x = dx;
        drag_event.y = dy;
        if (!triggerEvent(drag_event) && m_on_drag_move) {
            m_on_drag_move(this, dx, dy);
        }
    });
    
    m_frame_widget->setOnDragStart([this]() {
        bringToFront();
        
        // Trigger drag start event
        WindowEventData drag_event{};
        drag_event.type = WindowEvent::DRAG_START;
        if (!triggerEvent(drag_event) && m_on_drag_start) {
            m_on_drag_start(this, 0, 0); // Window-level drag, coordinates not meaningful
        }
    });
    
    m_frame_widget->setOnClose([this]() {
        close();
    });
    
    m_frame_widget->setOnMinimize([this]() {
        WindowEventData event_data{};
        event_data.type = WindowEvent::MINIMIZE;
        if (!triggerEvent(event_data) && m_on_minimize) {
            m_on_minimize(this);
        }
    });
    
    m_frame_widget->setOnMaximize([this]() {
        WindowEventData event_data{};
        event_data.type = WindowEvent::MAXIMIZE;
        if (!triggerEvent(event_data) && m_on_maximize) {
            m_on_maximize(this);
        }
    });
    
    m_frame_widget->setOnResize([this](int new_width, int new_height) {
        // Update our client area size
        m_client_width = new_width;
        m_client_height = new_height;
        
        // Update our own size to match the frame
        Rect frame_pos = m_frame_widget->getPos();
        setPos(frame_pos);
        
        // Trigger resize event
        WindowEventData event_data{};
        event_data.type = WindowEvent::RESIZE;
        event_data.width = new_width;
        event_data.height = new_height;
        if (!triggerEvent(event_data) && m_on_resize) {
            m_on_resize(this, new_width, new_height);
        }
    });
    
    // Add the frame widget as our child first
    addChild(std::move(m_frame_widget_owned));
    
    // Update layout to set our size to match the frame
    updateLayout();
}

void WindowWidget::BlitTo(Surface& target)
{
    // Use the standard Widget BlitTo method which handles coordinate translation properly
    Widget::BlitTo(target);
}

bool WindowWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Check for double-click (within 500ms and close proximity)
    uint32_t current_time = SDL_GetTicks();
    bool is_double_click = false;
    
    if (current_time - m_last_click_time < 500 && 
        abs(relative_x - m_last_click_x) < 5 && 
        abs(relative_y - m_last_click_y) < 5) {
        is_double_click = true;
    }
    
    m_last_click_time = current_time;
    m_last_click_x = relative_x;
    m_last_click_y = relative_y;
    
    // Convert SDL button to our convention (1=left, 2=middle, 3=right)
    int button = event.button;
    
    // Trigger click events
    WindowEventData event_data{};
    event_data.type = is_double_click ? WindowEvent::DOUBLE_CLICK_EVENT : WindowEvent::CLICK;
    event_data.x = relative_x;
    event_data.y = relative_y;
    event_data.button = button;
    
    // Call generic handler first - if set, it overrides specific handlers
    bool handled = triggerEvent(event_data);
    
    // If no generic handler, use specific handlers
    if (!m_on_event) {
        if (is_double_click && m_on_double_click) {
            m_on_double_click(this, relative_x, relative_y, button);
        } else if (!is_double_click && m_on_click) {
            m_on_click(this, relative_x, relative_y, button);
        }
    }
    
    // Bring window to front on any click
    bringToFront();
    
    // Forward to child widgets (especially the WindowFrameWidget) - this handles all frame interactions
    bool child_handled = Widget::handleMouseDown(event, relative_x, relative_y);
    
    return child_handled || handled;
}

bool WindowWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    // Forward to frame widget and child widgets
    return Widget::handleMouseMotion(event, relative_x, relative_y);
}

bool WindowWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Forward to frame widget and child widgets
    return Widget::handleMouseUp(event, relative_x, relative_y);
}

const std::string& WindowWidget::getTitle() const
{
    if (m_frame_widget) {
        return m_frame_widget->getTitle();
    }
    static const std::string empty_title;
    return empty_title;
}

void WindowWidget::setTitle(const std::string& title)
{
    if (m_frame_widget) {
        m_frame_widget->setTitle(title);
    }
}

Widget* WindowWidget::getClientArea() const
{
    if (m_frame_widget) {
        return m_frame_widget->getClientArea();
    }
    return nullptr;
}

void WindowWidget::setClientArea(std::unique_ptr<Widget> client_widget)
{
    if (m_frame_widget) {
        m_frame_widget->setClientArea(std::move(client_widget));
    }
}

void WindowWidget::bringToFront()
{
    m_z_order = s_next_z_order++;
}

void WindowWidget::updateLayout()
{
    if (m_frame_widget) {
        // Get current frame position and size
        Rect frame_pos = m_frame_widget->getPos();
        
        // Set our size and position to match the frame initially
        setPos(frame_pos);
        
        // Position the frame widget at (0,0) relative to us
        m_frame_widget->setPos(Rect(0, 0, frame_pos.width(), frame_pos.height()));
    }
}


// ========== EVENT SYSTEM IMPLEMENTATION ==========

void WindowWidget::close()
{
    // Trigger close event
    WindowEventData event_data{};
    event_data.type = WindowEvent::CLOSE;
    
    // Generic handler overrides specific handlers
    bool handled = triggerEvent(event_data);
    
    // Call specific handler if no generic handler
    if (!handled && !m_on_event && m_on_close) {
        m_on_close(this);
    }
    
    // Schedule removal from ApplicationWidget on next frame to avoid use-after-free
    ApplicationWidget::getInstance().scheduleWindowRemoval(this);
}

void WindowWidget::shutdown()
{
    // Trigger shutdown event
    WindowEventData event_data{};
    event_data.type = WindowEvent::SHUTDOWN;
    
    // Generic handler overrides specific handlers
    bool handled = triggerEvent(event_data);
    
    // If no specific shutdown handler, default to calling close handler/event
    if (!handled && !m_on_event) {
        if (m_on_shutdown) {
            m_on_shutdown(this);
        } else {
            // Default behavior: fall back to close handler if no shutdown handler
            if (m_on_close) {
                m_on_close(this);
            } else {
                // If no close handler either, trigger a close event through generic system
                WindowEventData close_event{};
                close_event.type = WindowEvent::CLOSE;
                triggerEvent(close_event);
            }
        }
    }
    
    // Note: We don't schedule removal here - shutdown is notification only
    // The window can decide whether to close or just clean up
}

void WindowWidget::triggerCustomEvent(void* custom_data)
{
    WindowEventData event_data{};
    event_data.type = WindowEvent::CUSTOM;
    event_data.custom_data = custom_data;
    
    triggerEvent(event_data);
}

bool WindowWidget::triggerEvent(const WindowEventData& event_data)
{
    // Call generic event handler if set
    if (m_on_event) {
        return m_on_event(this, event_data);
    }
    return false;
}

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3
