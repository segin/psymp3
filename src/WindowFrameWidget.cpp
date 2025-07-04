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
{
    // Calculate total window size (client area + decorations)
    int total_width = client_width + (RESIZE_BORDER_WIDTH * 2);
    int total_height = client_height + TITLEBAR_HEIGHT + (RESIZE_BORDER_WIDTH * 2);
    
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
        // Check if click is in titlebar
        if (isInTitlebar(relative_x, relative_y)) {
            // Check for control menu click
            Rect control_menu_bounds = getControlMenuBounds();
            if (relative_x >= control_menu_bounds.x() && relative_x < control_menu_bounds.x() + control_menu_bounds.width() &&
                relative_y >= control_menu_bounds.y() && relative_y < control_menu_bounds.y() + control_menu_bounds.height()) {
                
                Uint32 current_time = SDL_GetTicks();
                
                // Check for double-click (close window)
                if (m_double_click_pending && (current_time - m_last_click_time) < 500) {
                    // Double-click detected - close window
                    if (m_on_close) {
                        m_on_close();
                    }
                    m_double_click_pending = false;
                    m_system_menu_open = false;
                    rebuildSurface();
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
                    m_on_minimize();
                }
                return true;
            }
            
            // Check for maximize button click
            Rect maximize_bounds = getMaximizeButtonBounds();
            if (relative_x >= maximize_bounds.x() && relative_x < maximize_bounds.x() + maximize_bounds.width() &&
                relative_y >= maximize_bounds.y() && relative_y < maximize_bounds.y() + maximize_bounds.height()) {
                if (m_on_maximize) {
                    m_on_maximize();
                }
                return true;
            }
            
            // Check for double-click in draggable area (for close)
            if (isInDraggableArea(relative_x, relative_y)) {
                Uint32 current_time = SDL_GetTicks();
                if (m_double_click_pending && (current_time - m_last_click_time) < 500) {
                    // Double-click detected
                    if (m_on_close) {
                        m_on_close();
                    }
                    m_double_click_pending = false;
                    return true;
                } else {
                    // First click - start dragging and set up double-click detection
                    m_last_click_time = current_time;
                    m_double_click_pending = true;
                    
                    m_is_dragging = true;
                    m_last_mouse_x = event.x;
                    m_last_mouse_y = event.y;
                    
                    if (m_on_drag_start) {
                        m_on_drag_start();
                    }
                    
                    return true;
                }
            }
        }
        
        // Check if click is on resize border
        int resize_edge = getResizeEdge(relative_x, relative_y);
        if (resize_edge != 0) {
            m_is_resizing = true;
            m_resize_edge = resize_edge;
            m_resize_start_x = event.x;
            m_resize_start_y = event.y;
            m_resize_start_width = m_client_width;
            m_resize_start_height = m_client_height;
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
    
    // Close system menu if clicking anywhere else
    if (m_system_menu_open) {
        m_system_menu_open = false;
        rebuildSurface();
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
    
    if (m_is_resizing) {
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
        if (new_width < 100) new_width = 100;
        if (new_height < 50) new_height = 50;
        
        // Only resize if size actually changed
        if (new_width != m_client_width || new_height != m_client_height) {
            m_client_width = new_width;
            m_client_height = new_height;
            
            // Update layout and rebuild surface
            updateLayout();
            rebuildSurface();
            
            // Notify callback
            if (m_on_resize) {
                m_on_resize(new_width, new_height);
            }
        }
        
        return true;
    }
    
    return false;
}

bool WindowFrameWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        if (m_is_dragging) {
            m_is_dragging = false;
            return true;
        }
        
        if (m_is_resizing) {
            m_is_resizing = false;
            m_resize_edge = 0;
            return true;
        }
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
    // Validate client dimensions to prevent crashes
    if (m_client_width <= 0 || m_client_height <= 0 || m_client_width > 10000 || m_client_height > 10000) {
        // Use safe default values
        m_client_width = 300;
        m_client_height = 200;
    }
    
    // Calculate total window size (increase border for resize handles)
    int resize_border = RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (resize_border * 2);
    int total_height = m_client_height + TITLEBAR_HEIGHT + (resize_border * 2);
    
    // Create the window frame surface
    auto frame_surface = std::make_unique<Surface>(total_width, total_height, true);
    
    // Fill with window background color (light gray)
    uint32_t bg_color = frame_surface->MapRGB(192, 192, 192);
    frame_surface->FillRect(bg_color);
    
    // Draw Windows 3.1 style resize border with corner sections and gaps
    int corner_size = 8; // Size of corner resize handles (smaller for 2px border)
    
    // Outer border (dark gray frame)
    frame_surface->rectangle(0, 0, total_width - 1, total_height - 1, 64, 64, 64, 255);
    
    // Inner border  
    frame_surface->rectangle(resize_border - 1, resize_border - 1, 
                           total_width - resize_border, total_height - resize_border, 
                           128, 128, 128, 255);
    
    // Draw resize border sections with gaps (characteristic of Windows 3.1)
    // Top border sections (with gaps for corners)
    frame_surface->box(corner_size, 1, total_width - corner_size - 1, resize_border - 1, 192, 192, 192, 255);
    
    // Bottom border sections (with gaps for corners)
    frame_surface->box(corner_size, total_height - resize_border, 
                      total_width - corner_size - 1, total_height - 2, 192, 192, 192, 255);
    
    // Left border sections (with gaps for corners)
    frame_surface->box(1, corner_size, resize_border - 1, total_height - corner_size - 1, 192, 192, 192, 255);
    
    // Right border sections (with gaps for corners)
    frame_surface->box(total_width - resize_border, corner_size, 
                      total_width - 2, total_height - corner_size - 1, 192, 192, 192, 255);
    
    // Draw corner resize handles
    // Top-left corner
    frame_surface->box(1, 1, corner_size - 1, corner_size - 1, 160, 160, 160, 255);
    
    // Top-right corner
    frame_surface->box(total_width - corner_size, 1, total_width - 2, corner_size - 1, 160, 160, 160, 255);
    
    // Bottom-left corner
    frame_surface->box(1, total_height - corner_size, corner_size - 1, total_height - 2, 160, 160, 160, 255);
    
    // Bottom-right corner
    frame_surface->box(total_width - corner_size, total_height - corner_size, 
                      total_width - 2, total_height - 2, 160, 160, 160, 255);
    
    // Draw titlebar area (blue)
    frame_surface->box(resize_border, resize_border, 
                      total_width - resize_border - 1, resize_border + TITLEBAR_HEIGHT - 1, 
                      0, 0, 128, 255); // Classic Windows 3.1 blue
    
    // Draw titlebar border
    frame_surface->rectangle(resize_border, resize_border, 
                           total_width - resize_border - 1, resize_border + TITLEBAR_HEIGHT - 1, 
                           0, 0, 64, 255);
    
    // TODO: Add title text rendering when font access is available
    
    // Draw window control buttons
    drawWindowControls(*frame_surface);
    
    // Draw system menu if open
    if (m_system_menu_open) {
        drawSystemMenu(*frame_surface);
    }
    
    // Set the surface
    setSurface(std::move(frame_surface));
}

void WindowFrameWidget::updateLayout()
{
    // Position client area within the frame (account for resize border)
    if (m_client_area) {
        int resize_border = RESIZE_BORDER_WIDTH;
        Rect client_pos(resize_border,                    // X: after left resize border
                       resize_border + TITLEBAR_HEIGHT,  // Y: after top border and titlebar
                       m_client_width,                    // Width: client area width
                       m_client_height);                  // Height: client area height
        m_client_area->setPos(client_pos);
    }
}

bool WindowFrameWidget::isInTitlebar(int x, int y) const
{
    int resize_border = RESIZE_BORDER_WIDTH;
    return (x >= resize_border && 
            x < m_client_width + resize_border &&
            y >= resize_border && 
            y < resize_border + TITLEBAR_HEIGHT);
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
    int resize_border = RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (resize_border * 2);
    int button_x = total_width - resize_border - (BUTTON_SIZE * 2);
    int button_y = resize_border;
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getMaximizeButtonBounds() const
{
    int resize_border = RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (resize_border * 2);
    int button_x = total_width - resize_border - BUTTON_SIZE;
    int button_y = resize_border;
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getControlMenuBounds() const
{
    int resize_border = RESIZE_BORDER_WIDTH;
    int menu_x = resize_border;
    int menu_y = resize_border;
    
    return Rect(menu_x, menu_y, CONTROL_MENU_SIZE, CONTROL_MENU_SIZE);
}

void WindowFrameWidget::drawWindowControls(Surface& surface) const
{
    // Draw control menu box (full height square on left)
    Rect control_menu_bounds = getControlMenuBounds();
    
    // Control menu background (light gray)
    surface.box(control_menu_bounds.x(), control_menu_bounds.y(), 
               control_menu_bounds.x() + control_menu_bounds.width() - 1, 
               control_menu_bounds.y() + control_menu_bounds.height() - 1, 
               192, 192, 192, 255);
    
    // Control menu border (simple black outline)
    surface.rectangle(control_menu_bounds.x(), control_menu_bounds.y(), 
                     control_menu_bounds.x() + control_menu_bounds.width() - 1, 
                     control_menu_bounds.y() + control_menu_bounds.height() - 1, 
                     0, 0, 0, 255);
    
    // Draw a simple window icon representation (small rectangle in center)
    int icon_size = 8;
    int icon_x = control_menu_bounds.x() + (control_menu_bounds.width() - icon_size) / 2;
    int icon_y = control_menu_bounds.y() + (control_menu_bounds.height() - icon_size) / 2;
    surface.rectangle(icon_x, icon_y, icon_x + icon_size - 1, icon_y + icon_size - 1, 0, 0, 0, 255);
    
    // Draw minimize button (full height square)
    Rect minimize_bounds = getMinimizeButtonBounds();
    
    // Button background (light gray)
    surface.box(minimize_bounds.x(), minimize_bounds.y(), 
               minimize_bounds.x() + minimize_bounds.width() - 1, 
               minimize_bounds.y() + minimize_bounds.height() - 1, 
               192, 192, 192, 255);
    
    // Button border (black outline)
    surface.rectangle(minimize_bounds.x(), minimize_bounds.y(), 
                     minimize_bounds.x() + minimize_bounds.width() - 1, 
                     minimize_bounds.y() + minimize_bounds.height() - 1, 
                     0, 0, 0, 255);
    
    // Minimize symbol (downward pointing triangle ▼)
    int min_center_x = minimize_bounds.x() + minimize_bounds.width() / 2;
    int min_center_y = minimize_bounds.y() + minimize_bounds.height() / 2;
    int triangle_size = 6;
    
    // Triangle points for downward arrow
    Sint16 min_x1 = min_center_x - triangle_size;
    Sint16 min_y1 = min_center_y - triangle_size / 2;
    Sint16 min_x2 = min_center_x + triangle_size;
    Sint16 min_y2 = min_center_y - triangle_size / 2;
    Sint16 min_x3 = min_center_x;
    Sint16 min_y3 = min_center_y + triangle_size / 2;
    
    surface.filledTriangle(min_x1, min_y1, min_x2, min_y2, min_x3, min_y3, 0, 0, 0, 255);
    
    // Draw maximize button (full height square)
    Rect maximize_bounds = getMaximizeButtonBounds();
    
    // Button background (light gray)
    surface.box(maximize_bounds.x(), maximize_bounds.y(), 
               maximize_bounds.x() + maximize_bounds.width() - 1, 
               maximize_bounds.y() + maximize_bounds.height() - 1, 
               192, 192, 192, 255);
    
    // Button border (black outline)
    surface.rectangle(maximize_bounds.x(), maximize_bounds.y(), 
                     maximize_bounds.x() + maximize_bounds.width() - 1, 
                     maximize_bounds.y() + maximize_bounds.height() - 1, 
                     0, 0, 0, 255);
    
    // Maximize symbol (upward pointing triangle ▲)
    int max_center_x = maximize_bounds.x() + maximize_bounds.width() / 2;
    int max_center_y = maximize_bounds.y() + maximize_bounds.height() / 2;
    
    // Triangle points for upward arrow
    Sint16 max_x1 = max_center_x - triangle_size;
    Sint16 max_y1 = max_center_y + triangle_size / 2;
    Sint16 max_x2 = max_center_x + triangle_size;
    Sint16 max_y2 = max_center_y + triangle_size / 2;
    Sint16 max_x3 = max_center_x;
    Sint16 max_y3 = max_center_y - triangle_size / 2;
    
    surface.filledTriangle(max_x1, max_y1, max_x2, max_y2, max_x3, max_y3, 0, 0, 0, 255);
}

void WindowFrameWidget::drawSystemMenu(Surface& surface) const
{
    // Windows 3.1 system menu dimensions and styling
    const int menu_width = 120;
    const int menu_height = 140;
    const int shadow_offset = 2;
    
    // Draw dark grey drop shadow first
    surface.box(m_system_menu_x + shadow_offset, m_system_menu_y + shadow_offset,
               m_system_menu_x + menu_width + shadow_offset - 1, 
               m_system_menu_y + menu_height + shadow_offset - 1,
               64, 64, 64, 255);
    
    // Draw main menu background (light grey)
    surface.box(m_system_menu_x, m_system_menu_y,
               m_system_menu_x + menu_width - 1, m_system_menu_y + menu_height - 1,
               192, 192, 192, 255);
    
    // Draw black border around menu
    surface.rectangle(m_system_menu_x, m_system_menu_y,
                     m_system_menu_x + menu_width - 1, m_system_menu_y + menu_height - 1,
                     0, 0, 0, 255);
    
    // Menu item dimensions
    const int item_height = 16;
    const int separator_height = 4;
    int current_y = m_system_menu_y + 4;
    
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
            int sep_y = current_y + separator_height / 2;
            surface.hline(m_system_menu_x + 8, m_system_menu_x + menu_width - 8, sep_y - 1, 0, 0, 0, 255);
            surface.hline(m_system_menu_x + 8, m_system_menu_x + menu_width - 8, sep_y, 255, 255, 255, 255);
            surface.hline(m_system_menu_x + 8, m_system_menu_x + menu_width - 8, sep_y + 1, 0, 0, 0, 255);
            current_y += separator_height;
        } else {
            // Menu item area (could be highlighted if selected)
            // For now, just reserve the space - text rendering would go here
            current_y += item_height;
        }
    }
}

int WindowFrameWidget::getResizeEdge(int x, int y) const
{
    int resize_border = RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (resize_border * 2);
    int total_height = m_client_height + TITLEBAR_HEIGHT + (resize_border * 2);
    int corner_size = 8; // Match the corner size used in rebuildSurface
    
    int edge = 0;
    
    // Check for corner areas first (larger hit areas)
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
    
    // Check for edge areas
    if (x < resize_border) {
        edge |= 1; // Left edge
    } else if (x >= total_width - resize_border) {
        edge |= 2; // Right edge
    }
    
    if (y < resize_border) {
        edge |= 4; // Top edge
    } else if (y >= total_height - resize_border) {
        edge |= 8; // Bottom edge
    }
    
    return edge;
}