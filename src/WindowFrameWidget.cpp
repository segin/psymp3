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
                    m_double_click_pending = false;
                    m_system_menu_open = false;
                    if (m_on_close) {
                        m_on_close();
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
            
            // Check for click in draggable area (start dragging, no double-click close)
            if (isInDraggableArea(relative_x, relative_y)) {
                // Start dragging immediately 
                m_is_dragging = true;
                m_last_mouse_x = event.x;
                m_last_mouse_y = event.y;
                
                if (m_on_drag_start) {
                    m_on_drag_start();
                }
                
                return true;
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
    
    // Calculate total window size with proper Windows 3.1 structure:
    // Total = outer_border + resize_border + client_area + resize_border + outer_border
    // Titlebar = outer_border + resize_border + titlebar_total_height
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (total_border_width * 2);
    int total_height = m_client_height + TITLEBAR_TOTAL_HEIGHT + (total_border_width * 2);
    
    // Create the window frame surface
    auto frame_surface = std::make_unique<Surface>(total_width, total_height, true);
    
    // Fill with window background color (light gray)
    uint32_t bg_color = frame_surface->MapRGB(192, 192, 192);
    frame_surface->FillRect(bg_color);
    
    // Draw Windows 3.1 proper border structure
    // 1. Outer 1px frame around everything (dark gray)
    frame_surface->rectangle(0, 0, total_width - 1, total_height - 1, 64, 64, 64, 255);
    
    // 2. Inner resize frame (light gray - same as button background)
    int resize_inner = OUTER_BORDER_WIDTH;
    // Fill resize frame area with light gray
    frame_surface->box(resize_inner, resize_inner, 
                      total_width - resize_inner - 1, total_height - resize_inner - 1, 
                      192, 192, 192, 255);
    
    // Draw 1px black border around client/titlebar area 
    int content_outer = resize_inner + RESIZE_BORDER_WIDTH - 1;
    frame_surface->rectangle(content_outer, content_outer, 
                           total_width - content_outer - 1, total_height - content_outer - 1, 
                           0, 0, 0, 255);
    
    // 3. Draw titlebar area - shares border with resize frame
    int titlebar_x = resize_inner + RESIZE_BORDER_WIDTH;
    int titlebar_y = resize_inner + RESIZE_BORDER_WIDTH;
    int titlebar_width = total_width - (resize_inner + RESIZE_BORDER_WIDTH) * 2;
    int titlebar_height = TITLEBAR_TOTAL_HEIGHT;
    
    // Titlebar background (light gray)
    frame_surface->box(titlebar_x, titlebar_y, 
                      titlebar_x + titlebar_width - 1, titlebar_y + titlebar_height - 1, 
                      192, 192, 192, 255);
    
    // Titlebar borders (shared with resize frame on top/left/right)
    frame_surface->hline(titlebar_x, titlebar_x + titlebar_width - 1, titlebar_y, 64, 64, 64, 255); // Top
    frame_surface->vline(titlebar_x, titlebar_y, titlebar_y + titlebar_height - 1, 64, 64, 64, 255); // Left
    frame_surface->vline(titlebar_x + titlebar_width - 1, titlebar_y, titlebar_y + titlebar_height - 1, 64, 64, 64, 255); // Right
    frame_surface->hline(titlebar_x, titlebar_x + titlebar_width - 1, titlebar_y + titlebar_height - 1, 64, 64, 64, 255); // Bottom
    
    // Blue titlebar area (18px high, inside the 1px border)
    frame_surface->box(titlebar_x + 1, titlebar_y + 1, 
                      titlebar_x + titlebar_width - 2, titlebar_y + TITLEBAR_HEIGHT, 
                      0, 0, 128, 255); // Classic Windows 3.1 blue
    
    // 4. Draw client area border (shares border with resize frame and titlebar)
    int client_x = titlebar_x;
    int client_y = titlebar_y + titlebar_height;
    int client_width = titlebar_width;
    int client_height = m_client_height;
    
    // Client area borders (shared with resize frame on left/right/bottom, shared with titlebar on top)
    frame_surface->vline(client_x, client_y, client_y + client_height - 1, 64, 64, 64, 255); // Left
    frame_surface->vline(client_x + client_width - 1, client_y, client_y + client_height - 1, 64, 64, 64, 255); // Right  
    frame_surface->hline(client_x, client_x + client_width - 1, client_y + client_height - 1, 64, 64, 64, 255); // Bottom
    
    // 5. Draw corner separators aligned with minimize/maximize separator (black)
    // Calculate exact position to align with separator between minimize and maximize buttons
    Rect minimize_bounds_temp = getMinimizeButtonBounds();
    int corner_separator_x = minimize_bounds_temp.x() + BUTTON_SIZE; // Right edge of minimize button
    
    // Top-left corner notch (vertical separator aligned with button separator)
    frame_surface->vline(corner_separator_x, resize_inner, resize_inner + RESIZE_BORDER_WIDTH - 1, 0, 0, 0, 255);
    // Top-left corner notch (horizontal separator aligned with titlebar bottom)
    int titlebar_bottom_y = titlebar_y + TITLEBAR_TOTAL_HEIGHT - 1;
    frame_surface->hline(resize_inner, corner_separator_x, titlebar_bottom_y, 0, 0, 0, 255);
    
    // Top-right corner notch (vertical separator aligned with button separator, mirrored)
    frame_surface->vline(total_width - (corner_separator_x - resize_inner) - 1, resize_inner, resize_inner + RESIZE_BORDER_WIDTH - 1, 0, 0, 0, 255);
    // Top-right corner notch (horizontal separator aligned with titlebar bottom)
    frame_surface->hline(total_width - (corner_separator_x - resize_inner) - 1, total_width - resize_inner - 1, titlebar_bottom_y, 0, 0, 0, 255);
    
    // Bottom-left corner notch (vertical separator aligned with button separator)
    frame_surface->vline(corner_separator_x, total_height - resize_inner - RESIZE_BORDER_WIDTH, total_height - resize_inner - 1, 0, 0, 0, 255);
    // Bottom-left corner notch (horizontal separator aligned with titlebar bottom mirrored)
    frame_surface->hline(resize_inner, corner_separator_x, total_height - resize_inner - 1, 0, 0, 0, 255);
    
    // Bottom-right corner notch (vertical separator aligned with button separator, mirrored)
    frame_surface->vline(total_width - (corner_separator_x - resize_inner) - 1, total_height - resize_inner - RESIZE_BORDER_WIDTH, total_height - resize_inner - 1, 0, 0, 0, 255);
    // Bottom-right corner notch (horizontal separator aligned with titlebar bottom mirrored)
    frame_surface->hline(total_width - (corner_separator_x - resize_inner) - 1, total_width - resize_inner - 1, total_height - resize_inner - 1, 0, 0, 0, 255);
    
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
    // Position client area within the frame (account for borders and titlebar)
    if (m_client_area) {
        int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
        Rect client_pos(total_border_width,                           // X: after borders
                       total_border_width + TITLEBAR_TOTAL_HEIGHT,   // Y: after borders and titlebar
                       m_client_width,                               // Width: client area width
                       m_client_height);                             // Height: client area height
        m_client_area->setPos(client_pos);
    }
}

bool WindowFrameWidget::isInTitlebar(int x, int y) const
{
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    return (x >= total_border_width && 
            x < m_client_width + total_border_width &&
            y >= total_border_width && 
            y < total_border_width + TITLEBAR_TOTAL_HEIGHT);
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
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (total_border_width * 2);
    // Account for separator: minimize button is (BUTTON_SIZE * 2 + 1) from right edge  
    int button_x = total_width - total_border_width - (BUTTON_SIZE * 2) - 1;
    int button_y = total_border_width + 1; // Inside titlebar border
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getMaximizeButtonBounds() const
{
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (total_border_width * 2);
    int button_x = total_width - total_border_width - BUTTON_SIZE;
    int button_y = total_border_width + 1; // Inside titlebar border
    
    return Rect(button_x, button_y, BUTTON_SIZE, BUTTON_SIZE);
}

Rect WindowFrameWidget::getControlMenuBounds() const
{
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    int menu_x = total_border_width + 1; // Inside titlebar border
    int menu_y = total_border_width + 1; // Inside titlebar border
    
    return Rect(menu_x, menu_y, CONTROL_MENU_SIZE, CONTROL_MENU_SIZE);
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
    
    int icon_base_x = control_menu_bounds.x() + 3;
    int icon_base_y = control_menu_bounds.y() + 8;
    
    // Draw the white line from (3, 8) to (13, 8)
    surface.hline(icon_base_x, icon_base_x + 10, icon_base_y, 255, 255, 255, 255);
    
    // Draw 1px black border around the white line
    surface.hline(icon_base_x - 1, icon_base_x + 11, icon_base_y - 1, 0, 0, 0, 255); // Top border
    surface.hline(icon_base_x - 1, icon_base_x + 11, icon_base_y + 1, 0, 0, 0, 255); // Bottom border
    surface.pixel(icon_base_x - 1, icon_base_y, 0, 0, 0, 255); // Left border
    surface.pixel(icon_base_x + 11, icon_base_y, 0, 0, 0, 255); // Right border
    
    // Draw grey drop shadow starting at (3, 10), proceeding to (15, 10), then turning 90 degrees upward to end at (15, 8)
    surface.hline(icon_base_x, icon_base_x + 12, icon_base_y + 2, 128, 128, 128, 255); // Horizontal shadow line
    surface.vline(icon_base_x + 12, icon_base_y, icon_base_y + 2, 128, 128, 128, 255); // Vertical shadow line
    
    // Draw minimize button using reusable components
    Rect minimize_bounds = getMinimizeButtonBounds();
    
    // Draw 1px border between titlebar blue area and minimize button
    surface.vline(minimize_bounds.x() - 1, minimize_bounds.y(), minimize_bounds.y() + BUTTON_SIZE - 1, 64, 64, 64, 255);
    
    // Draw button background and 3D bevel
    drawButton(surface, minimize_bounds.x(), minimize_bounds.y(), BUTTON_SIZE, BUTTON_SIZE, false);
    
    // Draw minimize symbol (downward pointing triangle ▼)
    int min_center_x = minimize_bounds.x() + BUTTON_SIZE / 2;
    int min_center_y = minimize_bounds.y() + BUTTON_SIZE / 2;
    drawDownTriangle(surface, min_center_x, min_center_y, 3);
    
    // Draw maximize button using reusable components
    Rect maximize_bounds = getMaximizeButtonBounds();
    
    // Draw button background and 3D bevel
    drawButton(surface, maximize_bounds.x(), maximize_bounds.y(), BUTTON_SIZE, BUTTON_SIZE, false);
    
    // Draw maximize symbol (upward pointing triangle ▲)
    int max_center_x = maximize_bounds.x() + BUTTON_SIZE / 2;
    int max_center_y = maximize_bounds.y() + BUTTON_SIZE / 2;
    drawUpTriangle(surface, max_center_x, max_center_y, 3);
    
    // Draw 1px black separator between minimize and maximize buttons
    int separator_x = minimize_bounds.x() + minimize_bounds.width();
    surface.vline(separator_x, minimize_bounds.y(), minimize_bounds.y() + minimize_bounds.height() - 1, 0, 0, 0, 255);
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
    int total_border_width = OUTER_BORDER_WIDTH + RESIZE_BORDER_WIDTH;
    int total_width = m_client_width + (total_border_width * 2);
    int total_height = m_client_height + TITLEBAR_TOTAL_HEIGHT + (total_border_width * 2);
    int corner_size = 6; // Match the corner size used in rebuildSurface
    
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
    if (x < total_border_width) {
        edge |= 1; // Left edge
    } else if (x >= total_width - total_border_width) {
        edge |= 2; // Right edge
    }
    
    if (y < total_border_width) {
        edge |= 4; // Top edge
    } else if (y >= total_height - total_border_width) {
        edge |= 8; // Bottom edge
    }
    
    return edge;
}

void WindowFrameWidget::drawButton(Surface& surface, int x, int y, int width, int height, bool pressed)
{
    // Button background (light gray)
    surface.box(x, y, x + width - 1, y + height - 1, 192, 192, 192, 255);
    
    if (pressed) {
        // Pressed button - inverted bevel (dark on top/left, light on bottom/right)
        // Top and left shadow (dark gray)
        surface.hline(x, x + width - 2, y, 128, 128, 128, 255);
        surface.vline(x, y, y + height - 2, 128, 128, 128, 255);
        
        // Bottom and right highlight (white/light) - proper 3D bevel for pressed state
        surface.hline(x, x + width - 1, y + height - 1, 255, 255, 255, 255); // Start from left edge
        surface.vline(x + width - 1, y, y + height - 1, 255, 255, 255, 255); // Start from top edge
        
        // Additional darker shading row 1px inside from top and left (same color as main shadow)  
        // Start from edges for proper 3D bevel appearance in pressed state
        surface.hline(x, x + width - 2, y + 1, 128, 128, 128, 255); // Start from left edge
        surface.vline(x + 1, y, y + height - 2, 128, 128, 128, 255); // Start from top edge
    } else {
        // Normal button - standard 3D bevel (light on top/left, dark on bottom/right)
        // Top and left highlight (white/light)
        surface.hline(x, x + width - 2, y, 255, 255, 255, 255);
        surface.vline(x, y, y + height - 2, 255, 255, 255, 255);
        
        // Bottom and right shadow (dark gray) - proper 3D bevel extending to edges
        surface.hline(x, x + width - 1, y + height - 1, 128, 128, 128, 255); // Start from left edge
        surface.vline(x + width - 1, y, y + height - 1, 128, 128, 128, 255); // Start from top edge
        
        // Additional shading row 1px above bottom and 1px beside right (same color as main shadow)
        // Start from the very edges for proper 3D bevel appearance
        surface.hline(x, x + width - 2, y + height - 2, 128, 128, 128, 255); // Start from left edge
        surface.vline(x + width - 2, y, y + height - 2, 128, 128, 128, 255); // Start from top edge
    }
}

void WindowFrameWidget::drawDownTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Minimize triangle using one-indexed coordinates within button:
    // Top-left at (6, 8) in one-indexed = (5, 7) in zero-indexed
    // Convert center coordinates to button-relative coordinates
    Sint16 x1 = center_x - 4;  // Left point
    Sint16 y1 = center_y - 1;  // Top edge
    Sint16 x2 = center_x + 2;  // Right point  
    Sint16 y2 = center_y - 1;  // Top edge
    Sint16 x3 = center_x - 1;  // Bottom point
    Sint16 y3 = center_y + 2;  // Bottom edge
    
    surface.filledTriangle(x1, y1, x2, y2, x3, y3, 0, 0, 0, 255);
}

void WindowFrameWidget::drawUpTriangle(Surface& surface, int center_x, int center_y, int size)
{
    // Maximize triangle using one-indexed coordinates within button:
    // Control points (9, 6), (12, 10), (6, 10) in one-indexed
    // Convert to zero-indexed and adjust relative to center
    Sint16 x1 = center_x;      // Top point (9, 6)
    Sint16 y1 = center_y - 3;  
    Sint16 x2 = center_x + 3;  // Bottom-right point (12, 10)
    Sint16 y2 = center_y + 1;  
    Sint16 x3 = center_x - 3;  // Bottom-left point (6, 10)
    Sint16 y3 = center_y + 1;  
    
    surface.filledTriangle(x1, y1, x2, y2, x3, y3, 0, 0, 0, 255);
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
    // Draw restore symbol using specified coordinates:
    // Maximize arrow bottom-left at (6, 8) in one-indexed
    // Minimize arrow top-left at (6, 11) in one-indexed
    
    // Draw maximize triangle (bottom-left at (6, 8))
    Sint16 max_x1 = center_x - 3;      // Top point 
    Sint16 max_y1 = center_y - 1;      
    Sint16 max_x2 = center_x;          // Bottom-right point
    Sint16 max_y2 = center_y + 2;      
    Sint16 max_x3 = center_x - 6;      // Bottom-left point
    Sint16 max_y3 = center_y + 2;      
    surface.filledTriangle(max_x1, max_y1, max_x2, max_y2, max_x3, max_y3, 0, 0, 0, 255);
    
    // Draw minimize triangle (top-left at (6, 11))  
    Sint16 min_x1 = center_x - 6;      // Left point
    Sint16 min_y1 = center_y + 2;      // Top edge
    Sint16 min_x2 = center_x;          // Right point
    Sint16 min_y2 = center_y + 2;      // Top edge
    Sint16 min_x3 = center_x - 3;      // Bottom point
    Sint16 min_y3 = center_y + 5;      // Bottom edge
    surface.filledTriangle(min_x1, min_y1, min_x2, min_y2, min_x3, min_y3, 0, 0, 0, 255);
}