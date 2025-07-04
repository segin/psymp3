/*
 * ApplicationWidget.h - Root widget that manages the entire application UI
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

#ifndef APPLICATIONWIDGET_H
#define APPLICATIONWIDGET_H

#include "Widget.h"
#include "display.h"
#include <vector>
#include <memory>

/**
 * @brief Root widget that covers the entire SDL window and manages all UI elements.
 * 
 * This widget acts as the top-level container for all UI elements in the application.
 * It fills the entire SDL window and manages:
 * - The background/desktop UI (spectrum analyzer, controls, etc.)
 * - Window widgets (which appear on top of the background)
 * - Global mouse and keyboard event routing
 * - Z-order management for windows
 */
class ApplicationWidget : public Widget {
public:
    /**
     * @brief Constructor that creates an application widget covering the entire display.
     * @param display Reference to the display object for getting screen dimensions
     */
    ApplicationWidget(Display& display);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~ApplicationWidget() = default;
    
    /**
     * @brief Handles mouse button down events and routes them appropriately.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget (screen coordinates)
     * @param relative_y Y coordinate relative to this widget (screen coordinates)
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse motion events and routes them appropriately.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget (screen coordinates)
     * @param relative_y Y coordinate relative to this widget (screen coordinates)
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse button up events and routes them appropriately.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget (screen coordinates)
     * @param relative_y Y coordinate relative to this widget (screen coordinates)
     * @return true if the event was handled
     */
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Adds a window widget that should appear on top of the background.
     * @param window Window widget to add (takes ownership)
     */
    void addWindow(std::unique_ptr<Widget> window);
    
    /**
     * @brief Removes a window widget from the application.
     * @param window Pointer to the window widget to remove
     */
    void removeWindow(Widget* window);
    
    /**
     * @brief Sets the background/desktop widget (spectrum analyzer, controls, etc.).
     * @param background Background widget (takes ownership)
     */
    void setBackground(std::unique_ptr<Widget> background);
    
    /**
     * @brief Brings a window to the front (top of z-order).
     * @param window Pointer to the window widget to bring to front
     */
    void bringWindowToFront(Widget* window);

private:
    Display& m_display;
    std::unique_ptr<Widget> m_background;
    std::vector<std::unique_ptr<Widget>> m_windows;
    
    /**
     * @brief Finds the topmost window at the given coordinates.
     * @param x X coordinate
     * @param y Y coordinate
     * @return Pointer to the topmost window, or nullptr if none found
     */
    Widget* findWindowAt(int x, int y) const;
    
    /**
     * @brief Rebuilds the application surface by compositing all elements.
     */
    void rebuildSurface();
};

#endif // APPLICATIONWIDGET_H