/*
 * TitlebarWidget.h - Window titlebar widget with drag support
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

#ifndef TITLEBARWIDGET_H
#define TITLEBARWIDGET_H

// No direct includes - all includes should be in psymp3.h

class Font;

namespace PsyMP3 {
namespace Widget {
namespace Windowing {

/**
 * @brief A window titlebar widget that can be dragged to move its parent window.
 * 
 * TitlebarWidget provides a draggable titlebar with optional title text.
 * It uses callback functions to notify when dragging occurs.
 */
class TitlebarWidget : public Widget {
public:
    /**
     * @brief Constructor for TitlebarWidget.
     * @param width Titlebar width
     * @param height Titlebar height (typically 24px)
     * @param font Font to use for rendering title text
     * @param title Title text to display
     */
    TitlebarWidget(int width, int height, Font* font, const std::string& title = "");
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~TitlebarWidget() = default;
    
    /**
     * @brief Sets the title text.
     * @param title New title text
     */
    void setTitle(const std::string& title);
    
    /**
     * @brief Gets the title text.
     * @return Current title text
     */
    const std::string& getTitle() const { return m_title; }
    
    /**
     * @brief Sets the drag start callback.
     * @param callback Function called when drag starts (x, y are absolute coordinates)
     */
    void setOnDragStart(std::function<void(int x, int y)> callback) { 
        m_on_drag_start = callback; 
    }
    
    /**
     * @brief Sets the drag callback.
     * @param callback Function called during drag (dx, dy are relative movement)
     */
    void setOnDrag(std::function<void(int dx, int dy)> callback) { 
        m_on_drag = callback; 
    }
    
    /**
     * @brief Sets the drag end callback.
     * @param callback Function called when drag ends
     */
    void setOnDragEnd(std::function<void()> callback) { 
        m_on_drag_end = callback; 
    }
    
    /**
     * @brief Handles mouse button down events for drag initiation.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse motion events for dragging.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y);
    
    /**
     * @brief Handles mouse button up events for drag termination.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y);

private:
    /**
     * @brief Rebuilds the titlebar surface.
     */
    void rebuildSurface();
    
    Font* m_font;
    std::string m_title;
    int m_width;
    int m_height;
    
    // Drag state
    bool m_is_dragging;
    int m_last_mouse_x;
    int m_last_mouse_y;
    
    // Callbacks
    std::function<void(int x, int y)> m_on_drag_start;
    std::function<void(int dx, int dy)> m_on_drag;
    std::function<void()> m_on_drag_end;
};

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3

#endif // TITLEBARWIDGET_H