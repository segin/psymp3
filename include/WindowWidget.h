/*
 * WindowWidget.h - Floating window widget with modular titlebar and body
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

#ifndef WINDOWWIDGET_H
#define WINDOWWIDGET_H

#include "Widget.h"
#include "TitlebarWidget.h"
#include <string>
#include <memory>
#include <vector>

/**
 * @brief A floating window widget composed of titlebar and body widgets.
 * 
 * WindowWidget is a container that manages a titlebar widget and a body widget,
 * providing window decorations and drag functionality through composition.
 */
class WindowWidget : public Widget {
public:
    /**
     * @brief Constructor for WindowWidget.
     * @param width Total window width including borders
     * @param height Total window height including titlebar and borders
     * @param title Window title displayed in the titlebar
     */
    WindowWidget(int width, int height, const std::string& title = "");
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~WindowWidget() = default;
    
    /**
     * @brief Renders the window and all child widgets to the target surface.
     * @param target The surface to render to
     */
    virtual void BlitTo(Surface& target) override;
    
    /**
     * @brief Handles mouse button down events and forwards to child widgets.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse motion events and forwards to child widgets.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse button up events and forwards to child widgets.
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
    const std::string& getTitle() const;
    
    /**
     * @brief Sets the window title.
     * @param title The new window title
     */
    void setTitle(const std::string& title);
    
    /**
     * @brief Gets the body widget for custom content.
     * @return Pointer to the body widget
     */
    Widget* getBodyWidget() const { return m_body_widget.get(); }
    
    /**
     * @brief Sets a custom body widget.
     * @param body_widget The new body widget (window takes ownership)
     */
    void setBodyWidget(std::unique_ptr<Widget> body_widget);
    
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
     * @brief Sets the z-order level of this window.
     * @param z_order New z-order level
     */
    void setZOrder(int z_order) { m_z_order = z_order; }

protected:
    /**
     * @brief Updates the layout of child widgets.
     */
    void updateLayout();

private:
    static constexpr int TITLEBAR_HEIGHT = 24;
    static constexpr int BORDER_WIDTH = 1;
    
    int m_window_width;
    int m_window_height;
    
    // Child widgets
    std::unique_ptr<TitlebarWidget> m_titlebar_widget;
    std::unique_ptr<Widget> m_body_widget;
    
    // Z-order for window layering
    int m_z_order;
    static int s_next_z_order;
    
    /**
     * @brief Creates a default white body widget.
     * @return Default body widget
     */
    std::unique_ptr<Widget> createDefaultBodyWidget();
    
    /**
     * @brief Creates the window border surface.
     * @return Window border surface
     */
    std::unique_ptr<Surface> createBorderSurface();
};

#endif // WINDOWWIDGET_H