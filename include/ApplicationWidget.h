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

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Root widget that covers the entire SDL window and manages all UI elements.
 * 
 * This widget acts as the top-level container for all UI elements in the application.
 * It fills the entire SDL window and manages:
 * - The background/desktop UI (spectrum analyzer, controls, etc.)
 * - Window widgets (which appear on top of the background)
 * - Global mouse and keyboard event routing
 * - Z-order management for windows
 * 
 * This is a singleton class - only one ApplicationWidget can exist at a time.
 */
class ApplicationWidget : public Widget {
public:
    /**
     * @brief Gets the singleton instance of ApplicationWidget.
     * @param display Reference to the display object for getting screen dimensions
     * @return Reference to the singleton ApplicationWidget instance
     */
    static ApplicationWidget& getInstance(Display& display);
    
    /**
     * @brief Gets the singleton instance of ApplicationWidget (must be initialized first).
     * @return Reference to the singleton ApplicationWidget instance
     */
    static ApplicationWidget& getInstance();
    
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
     * @brief Adds a window widget that should appear on top of the desktop.
     * Windows are separate from the desktop child widgets and always render on top.
     * @param window Window widget to add (takes ownership)
     * @param z_order Z-order level for the window (see ZOrder namespace)
     */
    void addWindow(std::unique_ptr<Widget> window, int z_order = 50);
    
    /**
     * @brief Removes a window widget from the application.
     * @param window Pointer to the window widget to remove
     */
    void removeWindow(Widget* window);
    
    /**
     * @brief Brings a window to the front (top of z-order).
     * @param window Pointer to the window widget to bring to front
     */
    void bringWindowToFront(Widget* window);
    
    /**
     * @brief Updates all windows (handles auto-dismiss for toasts, animations, etc.).
     * Call this regularly from the main loop.
     */
    void updateWindows();
    
    /**
     * @brief Removes all toast windows immediately.
     * Used when showing a new toast to replace existing ones.
     */
    void removeAllToasts();
    
    /**
     * @brief Schedules a window for removal on the next update cycle.
     * This prevents use-after-free when windows close themselves.
     * @param window Pointer to the window to remove
     */
    void scheduleWindowRemoval(Widget* window);
    
    /**
     * @brief Notifies all windows that the application is shutting down.
     * Each window receives a SHUTDOWN event and can clean up accordingly.
     */
    void notifyShutdown();
    
    /**
     * @brief Renders child widgets and windows to the target surface.
     * @param target The surface to render to
     */
    virtual void BlitTo(Surface& target) override;

private:
    /**
     * @brief Private constructor for singleton pattern.
     * @param display Reference to the display object for getting screen dimensions
     */
    ApplicationWidget(Display& display);
    
    Display& m_display;
    std::vector<std::unique_ptr<Widget>> m_windows;
    std::vector<Widget*> m_windows_to_remove; // Windows scheduled for removal
    
    static std::unique_ptr<ApplicationWidget> s_instance;
    
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