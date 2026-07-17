/*
 * WindowFrameWidget.cpp - Implementation for classic window frame widget
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
using Foundation::DrawableWidget;



int WindowFrameWidget::s_next_z_order = 1;
int WindowFrameWidget::s_instance_count = 0;
SDL_Cursor* WindowFrameWidget::s_cursor_nwse = nullptr;
SDL_Cursor* WindowFrameWidget::s_cursor_nesw = nullptr;
SDL_Cursor* WindowFrameWidget::s_cursor_ew = nullptr;
SDL_Cursor* WindowFrameWidget::s_cursor_ns = nullptr;
SDL_Cursor* WindowFrameWidget::s_default_cursor = nullptr;
WindowFrameWidget* WindowFrameWidget::s_active_window = nullptr;
// Default maximize area = the full 640x404 base canvas; the Player narrows this
// to the region below the menu bar via setMaximizeArea().
Rect WindowFrameWidget::s_maximize_bounds = Rect(0, 0, 640, 404);

namespace {
// 16x16 standard cursor definitions
// NS: Double-headed vertical arrow
const Uint8 cursor_ns_data[] = {
    0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0xff, 0xff,
    0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80,
    0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};
const Uint8 cursor_ns_mask[] = {
    0x01, 0x80, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0,
    0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0xff, 0xff,
    0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0,
    0x03, 0xc0, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00
};

// EW: Double-headed horizontal arrow
const Uint8 cursor_ew_data[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10,
    0x0c, 0x30, 0x0e, 0x70, 0xff, 0xff, 0x0e, 0x70,
    0x0c, 0x30, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const Uint8 cursor_ew_mask[] = {
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x30, 0x1c, 0x38,
    0x1e, 0x78, 0x1f, 0xf8, 0xff, 0xff, 0x1f, 0xf8,
    0x1e, 0x78, 0x1c, 0x38, 0x0c, 0x30, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// NWSE: Diagonal
const Uint8 cursor_nwse_data[] = {
    0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x7c, 0x00,
    0x38, 0x00, 0x11, 0x00, 0x00, 0x88, 0x00, 0x44,
    0x00, 0x22, 0x00, 0x11, 0x00, 0x38, 0x00, 0x7c,
    0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const Uint8 cursor_nwse_mask[] = {
    0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x7e, 0x00,
    0x3c, 0x00, 0x1b, 0x80, 0x01, 0xdc, 0x00, 0xee,
    0x00, 0x77, 0x00, 0x3b, 0x00, 0x7e, 0x00, 0xff,
    0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// NESW: Diagonal
const Uint8 cursor_nesw_data[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x3e,
    0x00, 0x1c, 0x00, 0x88, 0x11, 0x00, 0x22, 0x00,
    0x44, 0x00, 0x88, 0x00, 0x1c, 0x00, 0x3e, 0x00,
    0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const Uint8 cursor_nesw_mask[] = {
    0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x7e,
    0x00, 0x3c, 0x01, 0xd8, 0x3b, 0x80, 0x77, 0x00,
    0xee, 0x00, 0xdc, 0x00, 0x3c, 0x00, 0x7e, 0x00,
    0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00
};
}

WindowFrameWidget::WindowFrameWidget(int client_width, int client_height, const std::string& title, Font* font)
    : Widget()
    , m_title(title)
    , m_font(font)
    , m_client_width(client_width)
    , m_client_height(client_height)
    , m_z_order(s_next_z_order++)
    , m_is_dragging(false)
    , m_last_mouse_x(0)
    , m_last_mouse_y(0)
    , m_last_click_time(0)
    , m_double_click_pending(false)
    , m_is_resizing(false)
    , m_resize_edge(0)
    , m_resize_start_x(0)
    , m_resize_start_y(0)
    , m_resize_start_width(0)
    , m_resize_start_height(0)
    , m_system_menu_open(false)
    , m_system_menu_x(0)
    , m_system_menu_y(0)
    , m_resizable(true)
    , m_minimizable(true)
    , m_maximizable(true)
    , m_maximized(false)
    , m_restore_bounds(0, 0, 0, 0)
{
    // Calculate total window size (client area + decorations)
    int horizontal_border_total, vertical_border_total;
    
    if (m_resizable) {
        // Resizable: (1px outer + 2px resize + 1px inner) * 2 sides = 8px
        horizontal_border_total = (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2;
        // Resizable: titlebar + borders + resize frames = 27px
        vertical_border_total = TITLEBAR_HEIGHT + (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2 + 1;
    } else {
        // Non-resizable: simple 1px border on all sides = 2px
        horizontal_border_total = 2;
        // Non-resizable: titlebar + simple borders = 21px
        vertical_border_total = TITLEBAR_HEIGHT + 2 + 1; // +1 for titlebar separator
    }
    // Calculate final window size
    int total_width = client_width + horizontal_border_total;
    int total_height = client_height + vertical_border_total;
    
    // Set initial position and size
    Rect pos(DEFAULT_WINDOW_X, DEFAULT_WINDOW_Y, total_width, total_height);
    setPos(pos);
    
    // Create default client area
    auto client_area = createDefaultClientArea();
    m_client_area = client_area.get();
    
    // Add client area as child widget
    addChild(std::move(client_area));
    
    // Force a complete refresh to ensure consistent initialization
    refresh();
    setActiveWindow(this);

    // Manage cursor resources
    s_instance_count++;
    if (!s_cursor_nwse) {
        s_default_cursor = SDL_GetCursor();
        // SDL 1.2 doesn't support SDL_CreateSystemCursor.
        // Create custom cursors for SDL 1.2 using SDL_CreateCursor
        s_cursor_nwse = SDL_CreateCursor(const_cast<Uint8*>(cursor_nwse_data), const_cast<Uint8*>(cursor_nwse_mask), 16, 16, 7, 7);
        s_cursor_nesw = SDL_CreateCursor(const_cast<Uint8*>(cursor_nesw_data), const_cast<Uint8*>(cursor_nesw_mask), 16, 16, 8, 8);
        s_cursor_ew = SDL_CreateCursor(const_cast<Uint8*>(cursor_ew_data), const_cast<Uint8*>(cursor_ew_mask), 16, 16, 8, 8);
        s_cursor_ns = SDL_CreateCursor(const_cast<Uint8*>(cursor_ns_data), const_cast<Uint8*>(cursor_ns_mask), 16, 16, 8, 8);
    }
}

WindowFrameWidget::~WindowFrameWidget()
{
    if (s_active_window == this) {
        s_active_window = nullptr;
    }
    s_instance_count--;
    if (s_instance_count == 0) {
        restoreDefaultCursor();
        if (s_cursor_nwse) {
            SDL_FreeCursor(s_cursor_nwse);
            s_cursor_nwse = nullptr;
        }
        if (s_cursor_nesw) {
            SDL_FreeCursor(s_cursor_nesw);
            s_cursor_nesw = nullptr;
        }
        if (s_cursor_ew) {
            SDL_FreeCursor(s_cursor_ew);
            s_cursor_ew = nullptr;
        }
        if (s_cursor_ns) {
            SDL_FreeCursor(s_cursor_ns);
            s_cursor_ns = nullptr;
        }
        s_default_cursor = SDL_GetCursor();
    }
}

bool WindowFrameWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        setActiveWindow(this);

        // Check if click is in titlebar
        if (isInTitlebar(relative_x, relative_y)) {
            // Check for control menu click
            Rect control_menu_bounds = getControlMenuBounds();
            if (relative_x >= control_menu_bounds.x() && relative_x < control_menu_bounds.x() + control_menu_bounds.width() &&
                relative_y >= control_menu_bounds.y() && relative_y < control_menu_bounds.y() + control_menu_bounds.height()) {
                
                Uint32 current_time = SDL_GetTicks();
                
                // Check for double-click (close window)
                if (m_double_click_pending && (current_time - m_last_click_time) < DOUBLE_CLICK_TIME_MS) {
                    // Double-click detected - close window
                    m_double_click_pending = false;
                    m_system_menu_open = false;
                    if (m_on_close) {
                        // Copy the callback to the stack before invoking: it may
                        // destroy this widget (and thus the m_on_close member),
                        // which would free the std::function mid-call. After the
                        // call, `this` may be dangling, so touch no members.
                        auto on_close = m_on_close;
                        on_close();
                    }
                    return true;
                } else {
                    // Single click - toggle system menu
                    m_last_click_time = current_time;
                    m_double_click_pending = true;
                    
                    m_system_menu_open = !m_system_menu_open;
                    if (m_system_menu_open) {
                        m_system_menu_x = control_menu_bounds.x();
                        m_system_menu_y = control_menu_bounds.y() + control_menu_bounds.height();
                    }
                    
                    // Rebuild surface to show/hide menu
                    rebuildSurface();
                    
                    if (m_on_control_menu) {
                        m_on_control_menu();
                    }
                    return true;
                }
            }
            
            // Check for minimize button click
            Rect minimize_bounds = getMinimizeButtonBounds();
            if (relative_x >= minimize_bounds.x() && relative_x < minimize_bounds.x() + minimize_bounds.width() &&
                relative_y >= minimize_bounds.y() && relative_y < minimize_bounds.y() + minimize_bounds.height()) {
                if (m_on_minimize) {
                    auto on_minimize = m_on_minimize;
                    on_minimize();
                }
                return true;
            }
            
            // Check for maximize button click
            Rect maximize_bounds = getMaximizeButtonBounds();
            if (relative_x >= maximize_bounds.x() && relative_x < maximize_bounds.x() + maximize_bounds.width() &&
                relative_y >= maximize_bounds.y() && relative_y < maximize_bounds.y() + maximize_bounds.height()) {
                toggleMaximize();
                if (m_on_maximize) {
                    auto on_maximize = m_on_maximize;
                    on_maximize();
                }
                return true;
            }
            
            // Check for click in draggable area (start dragging, no double-click
            // close). A maximized window is pinned in place, so no dragging.
            if (!m_maximized && isInDraggableArea(relative_x, relative_y)) {
                // Start dragging immediately with absolute coordinates. Use the
                // event's logical coords rather than SDL_GetMouseState (which
                // returns raw window pixels and breaks with display scaling).
                m_is_dragging = true;
                m_last_mouse_x = event.x;
                m_last_mouse_y = event.y;
                captureMouse(); // Capture mouse for global tracking
                
                if (m_on_drag_start) {
                    m_on_drag_start();
                }
                
                return true;
            }
        }
        
        // Check if click is on resize border (only for resizable, non-maximized
        // windows — a maximized window can't be edge-resized).
        if (m_resizable && !m_maximized) {
            int resize_edge = getResizeEdge(relative_x, relative_y);
            if (resize_edge != 0) {
                m_is_resizing = true;
                m_resize_edge = resize_edge;
                m_resize_start_x = event.x;
                m_resize_start_y = event.y;
                m_resize_start_width = m_client_width;
                m_resize_start_height = m_client_height;
                // Store original window position when resize starts
                Rect current_pos = getPos();
                m_resize_start_window_x = current_pos.x();
                m_resize_start_window_y = current_pos.y();
                captureMouse(); // Capture mouse for global tracking
                return true;
            }
        }
    }

    // Any button (not just left): bring the window to front and route into the
    // client area, so right-clicks — e.g. a ListView context menu — reach the
    // client. The titlebar/resize handling above stays left-only.
    bringToFront();

    if (m_client_area) {
        const Rect& client_pos = m_client_area->getPos();
        if (relative_x >= client_pos.x() &&
            relative_x < client_pos.x() + client_pos.width() &&
            relative_y >= client_pos.y() &&
            relative_y < client_pos.y() + client_pos.height()) {
            return m_client_area->handleMouseDown(
                event,
                relative_x - client_pos.x(),
                relative_y - client_pos.y());
        }
    }

    if (Widget::handleMouseDown(event, relative_x, relative_y)) {
        return true;
    }
    
    // Close system menu if clicking anywhere else
    if (m_system_menu_open) {
        m_system_menu_open = false;
        rebuildSurface();
    }
    
    return false;
}

bool WindowFrameWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    // Set appropriate cursor based on location
    if (!m_is_dragging && !m_is_resizing && m_resizable) {
        int resize_edge = getResizeEdge(relative_x, relative_y);
        if (resize_edge != 0) {
            // Set resize cursor based on edge
            if ((resize_edge & 1) && (resize_edge & 4)) { // Top-left corner
                setCursorShape(s_cursor_nwse);
            } else if ((resize_edge & 2) && (resize_edge & 4)) { // Top-right corner
                setCursorShape(s_cursor_nesw);
            } else if ((resize_edge & 1) && (resize_edge & 8)) { // Bottom-left corner
                setCursorShape(s_cursor_nesw);
            } else if ((resize_edge & 2) && (resize_edge & 8)) { // Bottom-right corner
                setCursorShape(s_cursor_nwse);
            } else if (resize_edge & 3) { // Left or right edge
                setCursorShape(s_cursor_ew);
            } else if (resize_edge & 12) { // Top or bottom edge
                setCursorShape(s_cursor_ns);
            }
        } else {
            restoreDefaultCursor();
        }
    } else if (!m_is_dragging && !m_is_resizing) {
        restoreDefaultCursor();
    }
    
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
    
    if (m_is_resizing) {
        // Use the event's logical coordinates for resize tracking so display
        // scaling doesn't desync the deltas from the captured start position.
        int dx = event.x - m_resize_start_x;
        int dy = event.y - m_resize_start_y;
        
        int new_width = m_resize_start_width;
        int new_height = m_resize_start_height;
        
        // Calculate new size based on resize edge
        if (m_resize_edge & 1) { // Left edge
            new_width = m_resize_start_width - dx;
        }
        if (m_resize_edge & 2) { // Right edge
            new_width = m_resize_start_width + dx;
        }
        if (m_resize_edge & 4) { // Top edge
            new_height = m_resize_start_height - dy;
        }
        if (m_resize_edge & 8) { // Bottom edge
            new_height = m_resize_start_height + dy;
        }
        
        // Enforce minimum size
        if (new_width < MIN_CLIENT_WIDTH) new_width = MIN_CLIENT_WIDTH;
        if (new_height < MIN_CLIENT_HEIGHT) new_height = MIN_CLIENT_HEIGHT;
        
        // Only resize if size actually changed
        if (new_width != m_client_width || new_height != m_client_height) {
            m_client_width = new_width;
            m_client_height = new_height;
            
            // Update window total size based on new client size
            int horizontal_border_total, vertical_border_total;
            
            if (m_resizable) {
                // Resizable: (1px outer + 2px resize + 1px inner) * 2 sides = 8px
                horizontal_border_total = (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2;
                // Resizable: titlebar + borders + resize frames = 27px
                vertical_border_total = TITLEBAR_HEIGHT + (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2 + 1;
            } else {
                // Non-resizable: simple 1px border on all sides = 2px
                horizontal_border_total = 2;
                // Non-resizable: titlebar + simple borders = 21px
                vertical_border_total = TITLEBAR_HEIGHT + 2 + 1; // +1 for titlebar separator
            }
            // Calculate final window size
            int total_width = m_client_width + horizontal_border_total;
            int total_height = m_client_height + vertical_border_total;
            
            // Calculate new window position - adjust for top/left edge resizing
            // Use original window position when resize started to avoid accumulating errors
            int new_x = m_resize_start_window_x;
            int new_y = m_resize_start_window_y;
            
            // When resizing from left edge, adjust position to keep right edge fixed
            if (m_resize_edge & 1) { // Left edge
                new_x += (m_resize_start_width - new_width);
            }
            
            // When resizing from top edge, adjust position to keep bottom edge fixed  
            if (m_resize_edge & 4) { // Top edge
                new_y += (m_resize_start_height - new_height);
            }
            
            // Update window size and position
            setPos(Rect(new_x, new_y, total_width, total_height));
            
            // Update layout and rebuild surface
            updateLayout();
            rebuildSurface();
            
            // Force client area to repaint by rebuilding its surface if it exists
            if (m_client_area) {
                auto client_surface = std::make_unique<Surface>(m_client_width, m_client_height, true);
                uint32_t white_color = client_surface->MapRGB(255, 255, 255);
                client_surface->FillRect(white_color);
                m_client_area->setSurface(std::move(client_surface));
            }
            
            // Notify callback (copy to the stack first; it may destroy this
            // widget, after which no member may be touched).
            if (m_on_resize) {
                auto on_resize = m_on_resize;
                on_resize(new_width, new_height);
            }
        }
        
        return true;
    }
    
    return Widget::handleMouseMotion(event, relative_x, relative_y);
}

bool WindowFrameWidget::handleMouseWheel(int delta, int relative_x, int relative_y)
{
    if (m_client_area) {
        const Rect& client_pos = m_client_area->getPos();
        if (relative_x >= client_pos.x() && relative_x < client_pos.x() + client_pos.width() &&
            relative_y >= client_pos.y() && relative_y < client_pos.y() + client_pos.height()) {
            return m_client_area->handleMouseWheel(
                delta, relative_x - client_pos.x(), relative_y - client_pos.y());
        }
    }
    return false;
}

bool WindowFrameWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        if (m_is_dragging) {
            m_is_dragging = false;
            releaseMouse(); // Release mouse capture
            return true;
        }
        
        if (m_is_resizing) {
            m_is_resizing = false;
            m_resize_edge = 0;
            releaseMouse(); // Release mouse capture
            return true;
        }
    }
    
    return Widget::handleMouseUp(event, relative_x, relative_y);
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
    setActiveWindow(this);
}

bool WindowFrameWidget::isActive() const
{
    return s_active_window == this;
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
    // Validate client dimensions to prevent crashes
    if (m_client_width <= 0 || m_client_height <= 0 || m_client_width > MAX_CLIENT_DIMENSION || m_client_height > MAX_CLIENT_DIMENSION) {
        // Use safe default values
        m_client_width = DEFAULT_CLIENT_WIDTH;
        m_client_height = DEFAULT_CLIENT_HEIGHT;
    }
    
    // Calculate total window size based on window properties
    int horizontal_border_total, vertical_border_total;
    
    if (m_resizable) {
        // Resizable: (1px outer + 2px resize + 1px inner) * 2 sides = 8px
        horizontal_border_total = (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2;
        // Resizable: titlebar + borders + resize frames = 27px
        vertical_border_total = TITLEBAR_HEIGHT + (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2 + 1;
    } else {
        // Non-resizable: simple 1px border on all sides = 2px
        horizontal_border_total = 2;
        // Non-resizable: titlebar + simple borders = 21px
        vertical_border_total = TITLEBAR_HEIGHT + 2 + 1; // +1 for titlebar separator
    }
    // Calculate final window size
    int total_width = m_client_width + horizontal_border_total;
    int total_height = m_client_height + vertical_border_total;
    
    // Create the window frame surface
    auto frame_surface = std::make_unique<Surface>(total_width, total_height, true);
    
    // effective_resize_width already calculated above
    
    // Fill with window background color (light gray)
    uint32_t bg_color = frame_surface->MapRGB(192, 192, 192);
    frame_surface->FillRect(bg_color);
    
    // Draw border structure based on window type
    int content_x, content_y, content_width, content_height;
    
    if (m_resizable) {
        // Resizable window: complex border structure
        // Structure: [1px black outer] [2px light grey resize frame] [1px black inner] [content]
        
        // 1. Outer 1px black frame around everything
        frame_surface->rectangle(0, 0, total_width - 1, total_height - 1, 0, 0, 0, 255);
        
        // 2. Resize frame
        int resize_start = OUTER_BORDER_WIDTH;
        frame_surface->box(resize_start, resize_start, 
                          total_width - resize_start - 1, total_height - resize_start - 1, 
                          192, 192, 192, 255);
        
        // 3. Inner 1px black border around content area
        int content_border = resize_start + RESIZE_BORDER_WIDTH;
        frame_surface->rectangle(content_border, content_border, 
                               total_width - content_border - 1, total_height - content_border - 1, 
                               0, 0, 0, 255);
        
        // 4. Content area coordinates (inside the black border)
        content_x = content_border + 1;
        content_y = content_border + 1;
        content_width = total_width - (content_border + 1) * 2;
        content_height = total_height - (content_border + 1) * 2;
    } else {
        // Non-resizable window: simple 1px black border
        frame_surface->rectangle(0, 0, total_width - 1, total_height - 1, 0, 0, 0, 255);
        
        // Content area coordinates (directly inside the 1px border)
        content_x = 1;
        content_y = 1;
        content_width = total_width - 2;
        content_height = total_height - 2;
    }
    
    // Draw titlebar and client area
    const bool active = isActive();
    const uint8_t title_r = active ? 0 : 128;
    const uint8_t title_g = active ? 0 : 128;
    const uint8_t title_b = active ? 128 : 128;

    // Titlebar area (blue when active, grey when inactive)
    frame_surface->box(content_x, content_y, 
                      content_x + content_width - 1, content_y + TITLEBAR_HEIGHT - 1, 
                      title_r, title_g, title_b, 255);
    
    // 1px black border between titlebar and client area
    int client_y = content_y + TITLEBAR_HEIGHT;
    frame_surface->hline(content_x, content_x + content_width - 1, client_y, 0, 0, 0, 255);
    
    // Client area (white background, starting after the 1px black border)
    int client_area_y = client_y + 1;
    int client_height = content_height - TITLEBAR_HEIGHT - 1; // Account for 1px black border
    frame_surface->box(content_x, client_area_y, 
                      content_x + content_width - 1, client_area_y + client_height - 1, 
                      255, 255, 255, 255);
    
    // Draw corner separators for resizable windows only
    if (m_resizable) {
        // Following the web canvas example: notches connect outer and inner borders
        // Outer border coordinates
        int outer_x1 = 0;
        int outer_y1 = 0; 
        int outer_x2 = total_width - 1;
        int outer_y2 = total_height - 1;
        
        // Inner border coordinates  
        int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        int inner_x1 = content_border;
        int inner_y1 = content_border;
        int inner_x2 = total_width - content_border - 1;
        int inner_y2 = total_height - content_border - 1;
        
        // Notch positions: offset from inner border edges
        int left_notch_x = inner_x1 + NOTCH_OFFSET;
        int right_notch_x = inner_x2 - NOTCH_OFFSET;
        int top_notch_y = inner_y1 + NOTCH_OFFSET;
        int bottom_notch_y = inner_y2 - NOTCH_OFFSET;
        
        // Top notches (vertical) - connect outer to inner border
        frame_surface->vline(left_notch_x, outer_y1, inner_y1, 0, 0, 0, 255);
        frame_surface->vline(right_notch_x, outer_y1, inner_y1, 0, 0, 0, 255);
        
        // Bottom notches (vertical) - connect inner to outer border
        frame_surface->vline(left_notch_x, inner_y2, outer_y2, 0, 0, 0, 255);
        frame_surface->vline(right_notch_x, inner_y2, outer_y2, 0, 0, 0, 255);
        
        // Left notches (horizontal) - connect outer to inner border
        frame_surface->hline(outer_x1, inner_x1, top_notch_y, 0, 0, 0, 255);
        frame_surface->hline(outer_x1, inner_x1, bottom_notch_y, 0, 0, 0, 255);
        
        // Right notches (horizontal) - connect inner to outer border
        frame_surface->hline(inner_x2, outer_x2, top_notch_y, 0, 0, 0, 255);
        frame_surface->hline(inner_x2, outer_x2, bottom_notch_y, 0, 0, 0, 255);
    }
    
    // Render title text (ClearType/LCD, pre-blended against the titlebar colour)
    if (m_font && !m_title.empty()) {
        auto text_surface = m_font->RenderLCD(TagLib::String(m_title, TagLib::String::UTF8),
                                              255, 255, 255,
                                              title_r, title_g, title_b);
        if (text_surface) {
            // Center horizontally in the titlebar area
            int text_x = content_x + (content_width - text_surface->width()) / 2;

            // Center vertically in the titlebar
            int text_y = content_y + (TITLEBAR_HEIGHT - text_surface->height()) / 2;

            frame_surface->Blit(*text_surface, Rect(text_x, text_y, text_surface->width(), text_surface->height()));
        }
    }
    
    // Draw window control buttons
    drawWindowControls(*frame_surface);
    
    // Draw system menu if open
    if (m_system_menu_open) {
        drawSystemMenu(*frame_surface);
    }
    
    // Set the surface
    setSurface(std::move(frame_surface));
}

void WindowFrameWidget::refresh()
{
    // Force complete recalculation of window dimensions and layout
    // This replicates what happens during resize to ensure consistent state
    
    // Recalculate total window size based on current properties
    int horizontal_border_total, vertical_border_total;
    
    if (m_resizable) {
        // Resizable: (1px outer + 2px resize + 1px inner) * 2 sides = 8px
        horizontal_border_total = (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2;
        // Resizable: titlebar + borders + resize frames = 27px
        vertical_border_total = TITLEBAR_HEIGHT + (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2 + 1;
    } else {
        // Non-resizable: simple 1px border on all sides = 2px
        horizontal_border_total = 2;
        // Non-resizable: titlebar + simple borders = 21px
        vertical_border_total = TITLEBAR_HEIGHT + 2 + 1; // +1 for titlebar separator
    }
    
    // Calculate final window size
    int total_width = m_client_width + horizontal_border_total;
    int total_height = m_client_height + vertical_border_total;
    
    // Update window size (keep current position)
    Rect current_pos = getPos();
    setPos(Rect(current_pos.x(), current_pos.y(), total_width, total_height));
    
    // Update layout and rebuild surface (same order as resize)
    updateLayout();
    rebuildSurface();
    
    // Force client area to repaint by rebuilding its surface if it exists
    if (m_client_area) {
        auto client_surface = std::make_unique<Surface>(m_client_width, m_client_height, true);
        uint32_t white_color = client_surface->MapRGB(255, 255, 255);
        client_surface->FillRect(white_color);
        m_client_area->setSurface(std::move(client_surface));
    }
}

void WindowFrameWidget::setMaximizeArea(const Rect& area)
{
    s_maximize_bounds = area;
}

void WindowFrameWidget::setFrameBounds(const Rect& bounds)
{
    // Derive the client size from the requested outer size using the current
    // border metrics, so the frame ends up exactly filling `bounds`.
    int horizontal_border_total, vertical_border_total;
    if (m_resizable) {
        horizontal_border_total = (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2;
        vertical_border_total = TITLEBAR_HEIGHT + (OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1) * 2 + 1;
    } else {
        horizontal_border_total = 2;
        vertical_border_total = TITLEBAR_HEIGHT + 2 + 1;
    }

    m_client_width = std::max(MIN_CLIENT_WIDTH, bounds.width() - horizontal_border_total);
    m_client_height = std::max(MIN_CLIENT_HEIGHT, bounds.height() - vertical_border_total);

    setPos(Rect(bounds.x(), bounds.y(),
                m_client_width + horizontal_border_total,
                m_client_height + vertical_border_total));

    updateLayout();
    rebuildSurface();

    if (m_client_area) {
        auto client_surface = std::make_unique<Surface>(m_client_width, m_client_height, true);
        client_surface->FillRect(client_surface->MapRGB(255, 255, 255));
        m_client_area->setSurface(std::move(client_surface));
    }

    // Let the client re-flow to the new size (copy the callback first; it may
    // outlive nothing here, but keep the same discipline as the drag path).
    if (m_on_resize) {
        auto on_resize = m_on_resize;
        on_resize(m_client_width, m_client_height);
    }
}

void WindowFrameWidget::toggleMaximize()
{
    if (!m_maximizable) {
        return;
    }
    if (m_maximized) {
        m_maximized = false;
        setFrameBounds(m_restore_bounds);
    } else {
        m_restore_bounds = getPos();
        m_maximized = true;
        setFrameBounds(s_maximize_bounds);
    }
    // Repaint the maximize/restore glyph for the new state.
    rebuildSurface();
}

void WindowFrameWidget::updateLayout()
{
    // Position client area within the frame (account for borders and titlebar)
    if (m_client_area) {
        int content_x, content_y;
        
        if (m_resizable) {
            // Resizable window: complex border structure
            int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
            content_x = content_border + 1;  // Inside the black border
            content_y = content_border + 1 + TITLEBAR_HEIGHT + 1;  // After titlebar + 1px black border
        } else {
            // Non-resizable window: simple 1px border
            content_x = 1;  // Inside the 1px border
            content_y = 1 + TITLEBAR_HEIGHT + 1;  // After titlebar + 1px black border
        }
        
        Rect client_pos(content_x,        // X: after borders and frame
                       content_y,        // Y: after borders, titlebar, and separator
                       m_client_width,   // Width: client area width
                       m_client_height); // Height: client area height
        m_client_area->setPos(client_pos);
    }
}

void WindowFrameWidget::setActiveWindow(WindowFrameWidget* window)
{
    if (s_active_window == window) {
        return;
    }

    WindowFrameWidget* previous = s_active_window;
    s_active_window = window;

    if (previous) {
        previous->rebuildSurface();
    }
    if (s_active_window) {
        s_active_window->rebuildSurface();
    }
}

void WindowFrameWidget::restoreDefaultCursor()
{
    setCursorShape(s_default_cursor ? s_default_cursor : SDL_GetCursor());
}

void WindowFrameWidget::setCursorShape(SDL_Cursor* cursor)
{
    if (cursor) {
        SDL_SetCursor(cursor);
    }
}

bool WindowFrameWidget::isInTitlebar(int x, int y) const
{
    int content_x, content_y;
    
    if (m_resizable) {
        int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        content_x = content_border + 1;
        content_y = content_border + 1;
    } else {
        content_x = 1;
        content_y = 1;
    }
    
    return (x >= content_x && 
            x < content_x + m_client_width &&
            y >= content_y && 
            y < content_y + TITLEBAR_HEIGHT);
}

bool WindowFrameWidget::isInDraggableArea(int x, int y) const
{
    if (!isInTitlebar(x, y)) return false;
    
    // Exclude button and control menu areas from draggable area
    Rect control_menu_bounds = getControlMenuBounds();
    Rect minimize_bounds = getMinimizeButtonBounds();
    Rect maximize_bounds = getMaximizeButtonBounds();
    
    // Check if click is not in any control
    bool in_control_menu = (x >= control_menu_bounds.x() && x < control_menu_bounds.x() + control_menu_bounds.width() &&
                           y >= control_menu_bounds.y() && y < control_menu_bounds.y() + control_menu_bounds.height());
    bool in_minimize = (x >= minimize_bounds.x() && x < minimize_bounds.x() + minimize_bounds.width() &&
                       y >= minimize_bounds.y() && y < minimize_bounds.y() + minimize_bounds.height());
    bool in_maximize = (x >= maximize_bounds.x() && x < maximize_bounds.x() + maximize_bounds.width() &&
                       y >= maximize_bounds.y() && y < maximize_bounds.y() + maximize_bounds.height());
    
    return !in_control_menu && !in_minimize && !in_maximize;
}

Rect WindowFrameWidget::getMinimizeButtonBounds() const
{
    if (!m_minimizable) return Rect(0, 0, 0, 0);
    
    int content_x, content_y, content_width;
    Rect window_pos = getPos();
    int total_width = window_pos.width();
    
    if (m_resizable) {
        int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        content_x = content_border + 1;
        content_y = content_border + 1;
        content_width = total_width - (content_border + 1) * 2;
    } else {
        content_x = 1;
        content_y = 1;
        content_width = total_width - 2;
    }
    
    // Button positioning depends on what buttons are visible
    int buttons_width = 0;
    if (m_maximizable) buttons_width += BUTTON_SIZE + 1; // +1 for separator
    buttons_width += BUTTON_SIZE; // minimize button
    
    int button_x = content_x + content_width - buttons_width;
    int button_y = content_y; // At top of titlebar area
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getMaximizeButtonBounds() const
{
    if (!m_maximizable) return Rect(0, 0, 0, 0);
    
    int content_x, content_y, content_width;
    Rect window_pos = getPos();
    int total_width = window_pos.width();
    
    if (m_resizable) {
        int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        content_x = content_border + 1;
        content_y = content_border + 1;
        content_width = total_width - (content_border + 1) * 2;
    } else {
        content_x = 1;
        content_y = 1;
        content_width = total_width - 2;
    }
    
    int button_x = content_x + content_width - BUTTON_SIZE;
    int button_y = content_y; // At top of titlebar area
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getControlMenuBounds() const
{
    int content_x, content_y;
    
    if (m_resizable) {
        int content_border = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        content_x = content_border + 1;
        content_y = content_border + 1;
    } else {
        content_x = 1;
        content_y = 1;
    }
    
    return Rect(content_x, content_y, CONTROL_MENU_SIZE, CONTROL_MENU_SIZE);
}

void WindowFrameWidget::drawWindowControls(Surface& surface) const
{
    // Draw control menu box (full height square on left)
    Rect control_menu_bounds = getControlMenuBounds();
    
    // Control menu background (light gray) - fill the entire 18x18 area
    surface.box(control_menu_bounds.x(), control_menu_bounds.y(), 
               control_menu_bounds.x() + CONTROL_MENU_SIZE - 1, 
               control_menu_bounds.y() + CONTROL_MENU_SIZE - 1, 
               192, 192, 192, 255);
    
    // Control menu has no border (as per user requirements)
    
    // Draw the specific Windows 3.1 control menu icon:
    // 1x11px white line starting from (3, 8) inside the control, proceeding to (13, 8)
    // with 1px black border around this line
    // and grey drop shadow starting at (3, 10), proceeding to (15, 10), then turning 90 degrees upward to end at (15, 8)
    
    int icon_base_x = control_menu_bounds.x() + CONTROL_ICON_X_OFFSET;
    int icon_base_y = control_menu_bounds.y() + CONTROL_ICON_Y_OFFSET;
    
    // Draw the white line from offset position
    surface.hline(icon_base_x, icon_base_x + CONTROL_ICON_WIDTH, icon_base_y, 255, 255, 255, 255);
    
    // Draw 1px black border around the white line
    surface.hline(icon_base_x - 1, icon_base_x + CONTROL_ICON_WIDTH + 1, icon_base_y - 1, 0, 0, 0, 255); // Top border
    surface.hline(icon_base_x - 1, icon_base_x + CONTROL_ICON_WIDTH + 1, icon_base_y + 1, 0, 0, 0, 255); // Bottom border
    surface.pixel(icon_base_x - 1, icon_base_y, 0, 0, 0, 255); // Left border
    surface.pixel(icon_base_x + CONTROL_ICON_WIDTH + 1, icon_base_y, 0, 0, 0, 255); // Right border
    
    // Draw grey drop shadow
    surface.hline(icon_base_x, icon_base_x + CONTROL_SHADOW_WIDTH, icon_base_y + CONTROL_SHADOW_Y_OFFSET - CONTROL_ICON_Y_OFFSET, 128, 128, 128, 255); // Horizontal shadow line
    surface.vline(icon_base_x + CONTROL_SHADOW_WIDTH, icon_base_y, icon_base_y + CONTROL_SHADOW_Y_OFFSET - CONTROL_ICON_Y_OFFSET, 128, 128, 128, 255); // Vertical shadow line
    
    // Draw 1px black vertical separator line between control menu and titlebar proper
    surface.vline(control_menu_bounds.x() + CONTROL_MENU_SIZE, control_menu_bounds.y(), 
                 control_menu_bounds.y() + CONTROL_MENU_SIZE - 1, 0, 0, 0, 255);
    
    // Draw minimize button if minimizable
    if (m_minimizable) {
        Rect minimize_bounds = getMinimizeButtonBounds();
        
        // Draw 1px border between titlebar blue area and minimize button
        surface.vline(minimize_bounds.x() - 1, minimize_bounds.y(), minimize_bounds.y() + BUTTON_SIZE - 1, 64, 64, 64, 255);
        
        // Draw button background and 3D bevel
        drawButton(surface, minimize_bounds.x(), minimize_bounds.y(), BUTTON_SIZE, BUTTON_SIZE, false);
        
        // Draw minimize symbol (downward pointing triangle ▼)
        int min_center_x = minimize_bounds.x() + BUTTON_SIZE / 2 - TRIANGLE_CENTER_OFFSET;
        int min_center_y = minimize_bounds.y() + BUTTON_SIZE / 2 - TRIANGLE_CENTER_OFFSET;
        drawDownTriangle(surface, min_center_x, min_center_y, TRIANGLE_SIZE);
    }
    
    // Draw maximize button if maximizable
    if (m_maximizable) {
        Rect maximize_bounds = getMaximizeButtonBounds();
        
        // Draw button background and 3D bevel
        drawButton(surface, maximize_bounds.x(), maximize_bounds.y(), BUTTON_SIZE, BUTTON_SIZE, false);

        int max_center_x = maximize_bounds.x() + BUTTON_SIZE / 2 - TRIANGLE_CENTER_OFFSET;
        int max_center_y = maximize_bounds.y() + BUTTON_SIZE / 2 - TRIANGLE_CENTER_OFFSET;
        if (m_maximized) {
            // Maximized: the button restores. Show the up+down "restore" glyph.
            drawRestoreSymbol(surface, max_center_x, max_center_y, TRIANGLE_SIZE);
        } else {
            // Draw maximize symbol (upward pointing triangle ▲)
            drawUpTriangle(surface, max_center_x, max_center_y, TRIANGLE_SIZE);
        }
    }
    
    // Draw separator between buttons only if both are visible
    if (m_minimizable && m_maximizable) {
        Rect minimize_bounds = getMinimizeButtonBounds();
        int separator_x = minimize_bounds.x() + minimize_bounds.width();
        surface.vline(separator_x, minimize_bounds.y(), minimize_bounds.y() + minimize_bounds.height() - 1, 0, 0, 0, 255);
    }
}

void WindowFrameWidget::drawSystemMenu(Surface& surface) const
{
    
    // Draw dark grey drop shadow first
    surface.box(m_system_menu_x + SYSTEM_MENU_SHADOW_OFFSET, m_system_menu_y + SYSTEM_MENU_SHADOW_OFFSET,
               m_system_menu_x + SYSTEM_MENU_WIDTH + SYSTEM_MENU_SHADOW_OFFSET - 1, 
               m_system_menu_y + SYSTEM_MENU_HEIGHT + SYSTEM_MENU_SHADOW_OFFSET - 1,
               64, 64, 64, 255);
    
    // Draw main menu background (light grey)
    surface.box(m_system_menu_x, m_system_menu_y,
               m_system_menu_x + SYSTEM_MENU_WIDTH - 1, m_system_menu_y + SYSTEM_MENU_HEIGHT - 1,
               192, 192, 192, 255);
    
    // Draw black border around menu
    surface.rectangle(m_system_menu_x, m_system_menu_y,
                     m_system_menu_x + SYSTEM_MENU_WIDTH - 1, m_system_menu_y + SYSTEM_MENU_HEIGHT - 1,
                     0, 0, 0, 255);
    
    // Menu item positioning
    int current_y = m_system_menu_y + SYSTEM_MENU_TOP_MARGIN;
    
    // Define menu items with their text and separators
    const char* menu_items[] = {
        "Restore",
        "Move",
        "Size",
        "Minimize",
        "Maximize",
        "",  // Separator
        "Close    Alt+F4"
    };
    
    // Draw menu items
    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        if (strlen(menu_items[i]) == 0) {
            // Draw separator line (white bar in black border)
            int sep_y = current_y + SYSTEM_MENU_SEPARATOR_HEIGHT / 2;
            surface.hline(m_system_menu_x + SYSTEM_MENU_BORDER_MARGIN, m_system_menu_x + SYSTEM_MENU_WIDTH - SYSTEM_MENU_BORDER_MARGIN, sep_y - 1, 0, 0, 0, 255);
            surface.hline(m_system_menu_x + SYSTEM_MENU_BORDER_MARGIN, m_system_menu_x + SYSTEM_MENU_WIDTH - SYSTEM_MENU_BORDER_MARGIN, sep_y, 255, 255, 255, 255);
            surface.hline(m_system_menu_x + SYSTEM_MENU_BORDER_MARGIN, m_system_menu_x + SYSTEM_MENU_WIDTH - SYSTEM_MENU_BORDER_MARGIN, sep_y + 1, 0, 0, 0, 255);
            current_y += SYSTEM_MENU_SEPARATOR_HEIGHT;
        } else {
            // Menu item area (could be highlighted if selected)
            // For now, just reserve the space - text rendering would go here
            current_y += SYSTEM_MENU_ITEM_HEIGHT;
        }
    }
}

int WindowFrameWidget::getResizeEdge(int x, int y) const
{
    // Non-resizable windows can't be resized
    if (!m_resizable) return 0;
    
    Rect window_pos = getPos();
    int total_width = window_pos.width();
    int total_height = window_pos.height();
    int corner_size = CORNER_RESIZE_SIZE;
    
    // Resize area includes: 1px outer + 2px resize frame + 1px inner border = 4px total
    int resize_area_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH + 1; // Include inner border
    
    int edge = 0;
    
    // Check for corner areas first (larger hit areas for easier grabbing)
    if (x < corner_size && y < corner_size) {
        return 1 | 4; // Top-left corner
    }
    if (x >= total_width - corner_size && y < corner_size) {
        return 2 | 4; // Top-right corner
    }
    if (x < corner_size && y >= total_height - corner_size) {
        return 1 | 8; // Bottom-left corner
    }
    if (x >= total_width - corner_size && y >= total_height - corner_size) {
        return 2 | 8; // Bottom-right corner
    }
    
    // Check for edge areas (includes inner border for grabbing)
    if (x < resize_area_width) {
        edge |= 1; // Left edge
    } else if (x >= total_width - resize_area_width) {
        edge |= 2; // Right edge
    }
    
    if (y < resize_area_width) {
        edge |= 4; // Top edge
    } else if (y >= total_height - resize_area_width) {
        edge |= 8; // Bottom edge
    }
    
    return edge;
}

void WindowFrameWidget::setResizable(bool resizable)
{
    if (m_resizable != resizable) {
        m_resizable = resizable;
        
        // Force complete refresh to ensure consistent state after property change
        refresh();
    }
}

int WindowFrameWidget::getEffectiveResizeBorderWidth() const
{
    return m_resizable ? RESIZE_BORDER_WIDTH : 1;
}

void WindowFrameWidget::drawButton(Surface& surface, int x, int y, int width, int height, bool pressed)
{
    // Button background (light gray)
    surface.box(x, y, x + width - 1, y + height - 1, 192, 192, 192, 255);
    
    if (pressed) {
        // Pressed button - inverted bevel (dark on top/left, light on bottom/right)
        // Top and left shadow (dark gray) - with 45-degree corner cut
        surface.hline(x + 1, x + width - 2, y, 128, 128, 128, 255); // Top line: start at (2,1) in one-indexed
        surface.vline(x, y + 1, y + height - 2, 128, 128, 128, 255); // Left line: start at (1,2) in one-indexed
        
        // Bottom and right highlight (white/light) - exact coordinates as specified
        // Outer highlight lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(x, x + width - 1, y + height - 1, 255, 255, 255, 255); // (1,18)-(18,18)
        surface.vline(x + width - 1, y, y + height - 1, 255, 255, 255, 255); // (18,1)-(18,18)
        
        // Inner shadow lines: (2,2)-(17,2) and (2,2)-(2,17) for pressed state
        surface.hline(x + 1, x + width - 2, y + 1, 128, 128, 128, 255); // (2,2)-(17,2)
        surface.vline(x + 1, y + 1, y + height - 2, 128, 128, 128, 255); // (2,2)-(2,17)
    } else {
        // Normal button - standard 3D bevel (light on top/left, dark on bottom/right)
        // Top and left highlight (white/light) - covering the full corner
        surface.hline(x, x + width - 2, y, 255, 255, 255, 255); // Top line: start at (1,1) in one-indexed
        surface.vline(x, y, y + height - 2, 255, 255, 255, 255); // Left line: start at (1,1) in one-indexed
        
        // Bottom and right shadow (dark gray) - exact coordinates as specified
        // Outer shading lines: (1,18)-(18,18) and (18,1)-(18,18)
        surface.hline(x, x + width - 1, y + height - 1, 128, 128, 128, 255); // (1,18)-(18,18)
        surface.vline(x + width - 1, y, y + height - 1, 128, 128, 128, 255); // (18,1)-(18,18)
        
        // Inner shading lines: (2,17)-(17,17) and (17,2)-(17,17)
        surface.hline(x + 1, x + width - 2, y + height - 2, 128, 128, 128, 255); // (2,17)-(17,17)
        surface.vline(x + width - 2, y + 1, y + height - 2, 128, 128, 128, 255); // (17,2)-(17,17)
    }
}

void WindowFrameWidget::drawDownTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Minimize triangle - downward pointing
    // Draw relative to center position, using source material pattern
    // Working inversely from widest line as specified
    surface.hline(center_x - 3, center_x + 3, center_y - 1, 0, 0, 0, 255);  // 7 pixels wide - widest
    surface.hline(center_x - 2, center_x + 2, center_y, 0, 0, 0, 255);      // 5 pixels wide
    surface.hline(center_x - 1, center_x + 1, center_y + 1, 0, 0, 0, 255);  // 3 pixels wide  
    surface.pixel(center_x, center_y + 2, 0, 0, 0, 255);                    // 1 pixel - tip
}

void WindowFrameWidget::drawUpTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Maximize triangle - upward pointing  
    // Draw relative to center position, using source material pattern
    surface.pixel(center_x, center_y - 2, 0, 0, 0, 255);                    // 1 pixel - tip
    surface.hline(center_x - 1, center_x + 1, center_y - 1, 0, 0, 0, 255);  // 3 pixels wide  
    surface.hline(center_x - 2, center_x + 2, center_y, 0, 0, 0, 255);      // 5 pixels wide
    surface.hline(center_x - 3, center_x + 3, center_y + 1, 0, 0, 0, 255);  // 7 pixels wide - widest
}

void WindowFrameWidget::drawLeftTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Triangle points for left pointing arrow ◀
    Sint16 x1 = center_x + 1;
    Sint16 y1 = center_y - size;
    Sint16 x2 = center_x + 1;
    Sint16 y2 = center_y + size;
    Sint16 x3 = center_x - 2;
    Sint16 y3 = center_y;
    
    surface.filledTriangle(x1, y1, x2, y2, x3, y3, 0, 0, 0, 255);
}

void WindowFrameWidget::drawRightTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Triangle points for right pointing arrow ▶
    Sint16 x1 = center_x - 1;
    Sint16 y1 = center_y - size;
    Sint16 x2 = center_x - 1;
    Sint16 y2 = center_y + size;
    Sint16 x3 = center_x + 2;
    Sint16 y3 = center_y;
    
    surface.filledTriangle(x1, y1, x2, y2, x3, y3, 0, 0, 0, 255);
}

void WindowFrameWidget::drawRestoreSymbol(Surface& surface, int center_x, int center_y, int size)
{
    (void)size;
    // The "restore" glyph is the maximize (up) and minimize (down) arrows stacked
    // on one button, pointing both ways at once, separated by a 2px gap. The
    // up-arrow's widest row sits at center_y-2 and the down-arrow's at center_y+1,
    // leaving rows center_y-1 and center_y empty between them (the 2px gap).
    drawUpTriangle(surface, center_x, center_y - 3, TRIANGLE_SIZE);
    drawDownTriangle(surface, center_x, center_y + 2, TRIANGLE_SIZE);
}

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3
