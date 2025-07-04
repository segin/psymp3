/*
 * WindowFrameWidget.h - Classic window frame with titlebar and client area
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

#ifndef WINDOWFRAMEWIDGET_H
#define WINDOWFRAMEWIDGET_H

#include "Widget.h"
#include <string>
#include <memory>
#include <functional>

/**
 * @brief A classic window frame widget with titlebar and resize border.
 * 
 * This widget provides the window decorations (titlebar, borders) and wraps
 * a client area widget. The client area is positioned within the frame.
 */
class WindowFrameWidget : public Widget {
public:
    /**
     * @brief Constructor for WindowFrameWidget.
     * @param client_width Width of the client area
     * @param client_height Height of the client area
     * @param title Window title displayed in the titlebar
     */
    WindowFrameWidget(int client_width, int client_height, const std::string& title = "");
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~WindowFrameWidget() = default;
    
    /**
     * @brief Handles mouse button down events.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse motion events.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse button up events.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Gets the window title.
     * @return The window title string
     */
    const std::string& getTitle() const { return m_title; }
    
    /**
     * @brief Sets the window title.
     * @param title The new window title
     */
    void setTitle(const std::string& title);
    
    /**
     * @brief Gets the client area widget.
     * @return Pointer to the client area widget
     */
    Widget* getClientArea() const { return m_client_area; }
    
    /**
     * @brief Sets a custom client area widget.
     * @param client_widget The new client area widget (window takes ownership)
     */
    void setClientArea(std::unique_ptr<Widget> client_widget);
    
    /**
     * @brief Brings this window to the front (for z-order management).
     */
    void bringToFront();
    
    /**
     * @brief Gets the z-order level of this window.
     * @return Z-order level (higher values are in front)
     */
    int getZOrder() const { return m_z_order; }
    
    /**
     * @brief Sets drag callback for window movement.
     * @param callback Function to call when window is dragged
     */
    void setOnDrag(std::function<void(int dx, int dy)> callback) { m_on_drag = callback; }
    
    /**
     * @brief Sets drag start callback.
     * @param callback Function to call when dragging starts
     */
    void setOnDragStart(std::function<void()> callback) { m_on_drag_start = callback; }
    
    /**
     * @brief Sets close callback for double-click.
     * @param callback Function to call when window should close
     */
    void setOnClose(std::function<void()> callback) { m_on_close = callback; }
    
    /**
     * @brief Sets minimize button callback.
     * @param callback Function to call when minimize button is clicked
     */
    void setOnMinimize(std::function<void()> callback) { m_on_minimize = callback; }
    
    /**
     * @brief Sets maximize button callback.
     * @param callback Function to call when maximize button is clicked
     */
    void setOnMaximize(std::function<void()> callback) { m_on_maximize = callback; }
    
    /**
     * @brief Sets control menu callback.
     * @param callback Function to call when control menu is clicked
     */
    void setOnControlMenu(std::function<void()> callback) { m_on_control_menu = callback; }
    
    /**
     * @brief Sets resize callback.
     * @param callback Function to call when window is resized (new_width, new_height)
     */
    void setOnResize(std::function<void(int new_width, int new_height)> callback) { m_on_resize = callback; }

private:
    static constexpr int TITLEBAR_HEIGHT = 18;  // Windows 3.x blue area
    static constexpr int TITLEBAR_TOTAL_HEIGHT = 20;  // Blue area + 1px top + 1px bottom border
    static constexpr int OUTER_BORDER_WIDTH = 1;  // Outer frame around everything
    static constexpr int RESIZE_BORDER_WIDTH = 2;  // Resize frame interior thickness
    static constexpr int BUTTON_SIZE = 18;  // Square buttons, same as titlebar blue height
    static constexpr int CONTROL_MENU_SIZE = 18;  // Same as titlebar blue height
    
    std::string m_title;
    int m_client_width;
    int m_client_height;
    
    // Client area widget (positioned within the frame)
    Widget* m_client_area;
    
    // Z-order for window layering
    int m_z_order;
    static int s_next_z_order;
    
    // Drag state
    bool m_is_dragging;
    int m_last_mouse_x;
    int m_last_mouse_y;
    
    // Double-click detection for close
    Uint32 m_last_click_time;
    bool m_double_click_pending;
    
    // Resize state
    bool m_is_resizing;
    int m_resize_edge; // 0=none, 1=left, 2=right, 4=top, 8=bottom, combinations for corners
    int m_resize_start_x;
    int m_resize_start_y;
    int m_resize_start_width;
    int m_resize_start_height;
    
    // System menu state
    bool m_system_menu_open;
    int m_system_menu_x;
    int m_system_menu_y;
    
    // Drag callbacks
    std::function<void(int dx, int dy)> m_on_drag;
    std::function<void()> m_on_drag_start;
    
    // Window control callbacks
    std::function<void()> m_on_close;
    std::function<void()> m_on_minimize;
    std::function<void()> m_on_maximize;
    std::function<void()> m_on_control_menu;
    std::function<void(int new_width, int new_height)> m_on_resize;
    
    /**
     * @brief Creates a default white client area widget.
     * @return Default client area widget
     */
    std::unique_ptr<Widget> createDefaultClientArea();
    
    /**
     * @brief Rebuilds the window frame surface.
     */
    void rebuildSurface();
    
    /**
     * @brief Updates the layout of the client area.
     */
    void updateLayout();
    
    /**
     * @brief Checks if a point is in the titlebar area.
     * @param x X coordinate relative to this widget
     * @param y Y coordinate relative to this widget
     * @return true if point is in titlebar
     */
    bool isInTitlebar(int x, int y) const;
    
    /**
     * @brief Checks if a point is in the draggable area of titlebar.
     * @param x X coordinate relative to this widget
     * @param y Y coordinate relative to this widget
     * @return true if point is in draggable titlebar area
     */
    bool isInDraggableArea(int x, int y) const;
    
    /**
     * @brief Gets the bounds of the minimize button.
     * @return Rectangle of minimize button
     */
    Rect getMinimizeButtonBounds() const;
    
    /**
     * @brief Gets the bounds of the maximize button.
     * @return Rectangle of maximize button
     */
    Rect getMaximizeButtonBounds() const;
    
    /**
     * @brief Gets the bounds of the control menu box.
     * @return Rectangle of control menu box
     */
    Rect getControlMenuBounds() const;
    
    /**
     * @brief Draws window control buttons on the surface.
     * @param surface Surface to draw on
     */
    void drawWindowControls(Surface& surface) const;
    
    /**
     * @brief Draws the system menu with Windows 3.1 styling.
     * @param surface Surface to draw on
     */
    void drawSystemMenu(Surface& surface) const;
    
    /**
     * @brief Draws a Windows 3.1 style button with 3D bevel effects.
     * @param surface Surface to draw on
     * @param x Button x position
     * @param y Button y position
     * @param width Button width
     * @param height Button height
     * @param pressed Whether button appears pressed (inverted bevel)
     */
    static void drawButton(Surface& surface, int x, int y, int width, int height, bool pressed = false);
    
    /**
     * @brief Draws a downward pointing triangle (minimize symbol).
     * @param surface Surface to draw on
     * @param center_x Triangle center x coordinate
     * @param center_y Triangle center y coordinate
     * @param size Triangle size
     */
    static void drawDownTriangle(Surface& surface, int center_x, int center_y, int size);
    
    /**
     * @brief Draws an upward pointing triangle (maximize symbol).
     * @param surface Surface to draw on
     * @param center_x Triangle center x coordinate
     * @param center_y Triangle center y coordinate
     * @param size Triangle size
     */
    static void drawUpTriangle(Surface& surface, int center_x, int center_y, int size);
    
    /**
     * @brief Draws a left pointing triangle (scrollbar left/up button).
     * @param surface Surface to draw on
     * @param center_x Triangle center x coordinate
     * @param center_y Triangle center y coordinate
     * @param size Triangle size
     */
    static void drawLeftTriangle(Surface& surface, int center_x, int center_y, int size);
    
    /**
     * @brief Draws a right pointing triangle (scrollbar right/down button).
     * @param surface Surface to draw on
     * @param center_x Triangle center x coordinate
     * @param center_y Triangle center y coordinate
     * @param size Triangle size
     */
    static void drawRightTriangle(Surface& surface, int center_x, int center_y, int size);
    
    /**
     * @brief Draws a restore window symbol (two overlapping rectangles).
     * @param surface Surface to draw on
     * @param center_x Symbol center x coordinate
     * @param center_y Symbol center y coordinate
     * @param size Symbol size
     */
    static void drawRestoreSymbol(Surface& surface, int center_x, int center_y, int size);
    
    /**
     * @brief Determines which resize edge/corner the mouse is over.
     * @param x X coordinate relative to this widget
     * @param y Y coordinate relative to this widget
     * @return Resize edge flags (1=left, 2=right, 4=top, 8=bottom)
     */
    int getResizeEdge(int x, int y) const;
};

#endif // WINDOWFRAMEWIDGET_H