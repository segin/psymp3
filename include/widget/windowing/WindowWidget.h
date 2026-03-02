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

// No direct includes - all includes should be in psymp3.h

class Font;

namespace PsyMP3 {
namespace Widget {
namespace Windowing {

// Forward declaration
class WindowWidget;

/**
 * @brief Event types for generic window message processing.
 */
enum class WindowEvent {
    CLICK,          // Mouse click (any button)
    DOUBLE_CLICK_EVENT,   // Double click
    DRAG_START,     // Drag operation started
    DRAG_MOVE,      // Drag operation in progress
    DRAG_END,       // Drag operation ended
    CLOSE,          // Window close requested
    MINIMIZE,       // Window minimize requested
    MAXIMIZE,       // Window maximize requested
    RESIZE,         // Window resize
    FOCUS_GAINED,   // Window gained focus
    FOCUS_LOST,     // Window lost focus
    PAINT,          // Window needs repainting
    SHUTDOWN,       // Program shutdown - window should clean up
    CUSTOM          // Custom user-defined event
};

/**
 * @brief Event data structure for generic window events.
 */
struct WindowEventData {
    WindowEvent type;
    int x = 0, y = 0;           // Mouse coordinates or position data
    int width = 0, height = 0;  // Size data for resize events
    int button = 0;             // Mouse button (1=left, 2=middle, 3=right)
    void* custom_data = nullptr; // Custom data for user events
};

/**
 * @brief A complete window widget that contains a WindowFrameWidget plus client area.
 * 
 * WindowWidget is the main window class that manages:
 * - WindowFrameWidget for system-provided GUI decorations (titlebar, borders, buttons)
 * - Client area widget where application-specific content is placed
 * - Window positioning, sizing, and event handling
 * - Self-managed lifecycle with Windows 3.x style behaviors
 * 
 * The size of WindowWidget is the full window size (frame + client area).
 * The client area is a subset within the frame where widgets added by 
 * application code should be placed.
 */
class WindowWidget : public Widget {
public:
    /**
     * @brief Constructor for WindowWidget.
     * @param client_width Width of the client area (content area)
     * @param client_height Height of the client area (content area)
     * @param title Window title displayed in the titlebar
     * @param font Font used for the titlebar text
     */
    WindowWidget(int client_width, int client_height, const std::string& title = "", Font* font = nullptr);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~WindowWidget() = default;
    
    // ========== EVENT HANDLER SETTERS ==========
    
    /**
     * @brief Sets the generic event handler for Win32-style message processing.
     * @param handler Function called for all events: bool handler(WindowWidget* self, const WindowEventData& event)
     */
    void setOnEvent(std::function<bool(WindowWidget*, const WindowEventData&)> handler) { m_on_event = handler; }
    
    /**
     * @brief Sets the click event handler.
     * @param handler Function called on mouse clicks: void handler(WindowWidget* self, int x, int y, int button)
     */
    void setOnClick(std::function<void(WindowWidget*, int, int, int)> handler) { m_on_click = handler; }
    
    /**
     * @brief Sets the double-click event handler.
     * @param handler Function called on double clicks: void handler(WindowWidget* self, int x, int y, int button)
     */
    void setOnDoubleClick(std::function<void(WindowWidget*, int, int, int)> handler) { m_on_double_click = handler; }
    
    /**
     * @brief Sets the drag start event handler.
     * @param handler Function called when dragging starts: void handler(WindowWidget* self, int x, int y)
     */
    void setOnDragStart(std::function<void(WindowWidget*, int, int)> handler) { m_on_drag_start = handler; }
    
    /**
     * @brief Sets the drag move event handler.
     * @param handler Function called during dragging: void handler(WindowWidget* self, int dx, int dy)
     */
    void setOnDragMove(std::function<void(WindowWidget*, int, int)> handler) { m_on_drag_move = handler; }
    
    /**
     * @brief Sets the drag end event handler.
     * @param handler Function called when dragging ends: void handler(WindowWidget* self, int x, int y)
     */
    void setOnDragEnd(std::function<void(WindowWidget*, int, int)> handler) { m_on_drag_end = handler; }
    
    /**
     * @brief Sets the close event handler.
     * @param handler Function called when window close is requested: void handler(WindowWidget* self)
     */
    void setOnClose(std::function<void(WindowWidget*)> handler) { m_on_close = handler; }
    
    /**
     * @brief Sets the minimize event handler.
     * @param handler Function called when window minimize is requested: void handler(WindowWidget* self)
     */
    void setOnMinimize(std::function<void(WindowWidget*)> handler) { m_on_minimize = handler; }
    
    /**
     * @brief Sets the maximize event handler.
     * @param handler Function called when window maximize is requested: void handler(WindowWidget* self)
     */
    void setOnMaximize(std::function<void(WindowWidget*)> handler) { m_on_maximize = handler; }
    
    /**
     * @brief Sets the resize event handler.
     * @param handler Function called when window is resized: void handler(WindowWidget* self, int new_width, int new_height)
     */
    void setOnResize(std::function<void(WindowWidget*, int, int)> handler) { m_on_resize = handler; }
    
    /**
     * @brief Sets the shutdown event handler.
     * @param handler Function called when program is shutting down: void handler(WindowWidget* self)
     */
    void setOnShutdown(std::function<void(WindowWidget*)> handler) { m_on_shutdown = handler; }
    
    // ========== WINDOW MANAGEMENT METHODS ==========
    
    /**
     * @brief Closes this window and removes it from the application.
     */
    void close();
    
    /**
     * @brief Notifies the window that the program is shutting down.
     * If no specific shutdown handler is set, defaults to calling the close handler.
     */
    void shutdown();
    
    /**
     * @brief Triggers a custom event with user-defined data.
     * @param custom_data Custom data pointer
     */
    void triggerCustomEvent(void* custom_data = nullptr);
    
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
     * @brief Gets the client area widget where application content should be placed.
     * @return Pointer to the client area widget
     */
    Widget* getClientArea() const;
    
    /**
     * @brief Sets a custom client area widget.
     * @param client_widget The new client area widget (window takes ownership)
     */
    void setClientArea(std::unique_ptr<Widget> client_widget);
    
    /**
     * @brief Gets the WindowFrameWidget that provides system decorations.
     * @return Pointer to the frame widget
     */
    WindowFrameWidget* getFrameWidget() const { return m_frame_widget; }
    
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
    int m_client_width;
    int m_client_height;
    
    // The WindowFrameWidget provides system decorations (titlebar, borders, etc.)
    std::unique_ptr<WindowFrameWidget> m_frame_widget_owned;
    WindowFrameWidget* m_frame_widget; // Non-owning pointer for access
    
    // Z-order for window layering
    int m_z_order;
    static int s_next_z_order;
    
    // Event handlers
    std::function<bool(WindowWidget*, const WindowEventData&)> m_on_event;
    std::function<void(WindowWidget*, int, int, int)> m_on_click;
    std::function<void(WindowWidget*, int, int, int)> m_on_double_click;
    std::function<void(WindowWidget*, int, int)> m_on_drag_start;
    std::function<void(WindowWidget*, int, int)> m_on_drag_move;
    std::function<void(WindowWidget*, int, int)> m_on_drag_end;
    std::function<void(WindowWidget*)> m_on_close;
    std::function<void(WindowWidget*)> m_on_minimize;
    std::function<void(WindowWidget*)> m_on_maximize;
    std::function<void(WindowWidget*, int, int)> m_on_resize;
    std::function<void(WindowWidget*)> m_on_shutdown;
    
    // Internal state for event handling
    bool m_is_dragging = false;
    int m_drag_start_x = 0, m_drag_start_y = 0;
    uint32_t m_last_click_time = 0;
    int m_last_click_x = 0, m_last_click_y = 0;
    
    
    /**
     * @brief Triggers an event through both specific and generic handlers.
     * @param event_data Event data to send
     * @return true if event was handled by generic handler
     */
    bool triggerEvent(const WindowEventData& event_data);
};

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3

#endif // WINDOWWIDGET_H