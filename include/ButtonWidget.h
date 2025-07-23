/*
 * Button.h - Generic reusable button widget
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

#ifndef BUTTONWIDGET_H
#define BUTTONWIDGET_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Types of button symbols that can be drawn
 */
enum class ButtonSymbol {
    None,           // Plain button with no symbol
    Minimize,       // Downward triangle
    Maximize,       // Upward triangle
    Restore,        // Two overlapping triangles
    Close,          // X symbol
    ScrollUp,       // Upward triangle (scrollbar)
    ScrollDown,     // Downward triangle (scrollbar)
    ScrollLeft,     // Leftward triangle (scrollbar)
    ScrollRight     // Rightward triangle (scrollbar)
};

/**
 * @brief A generic reusable button widget with 3D appearance.
 * 
 * This widget provides button appearance and behavior with proper 3D bevel 
 * effects, mouse interaction, and various symbols for different button types
 * (minimize, maximize, scrollbar arrows, etc.).
 */
class ButtonWidget : public Widget {
public:
    /**
     * @brief Constructor for ButtonWidget.
     * @param width Button width
     * @param height Button height
     * @param symbol Symbol to draw on the button
     */
    ButtonWidget(int width, int height, ButtonSymbol symbol = ButtonSymbol::None);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~ButtonWidget() = default;
    
    /**
     * @brief Handles mouse button down events.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse button up events.
     * @param event The mouse button event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Handles mouse motion events for hover effects.
     * @param event The mouse motion event
     * @param relative_x X coordinate relative to this widget
     * @param relative_y Y coordinate relative to this widget
     * @return true if the event was handled
     */
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    
    /**
     * @brief Sets the button symbol.
     * @param symbol The symbol to draw on the button
     */
    void setSymbol(ButtonSymbol symbol);
    
    /**
     * @brief Gets the current button symbol.
     * @return The current symbol
     */
    ButtonSymbol getSymbol() const { return m_symbol; }
    
    /**
     * @brief Sets whether the button is enabled.
     * @param enabled If false, button appears grayed out and doesn't respond to clicks
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Gets whether the button is enabled.
     * @return True if button is enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Sets the click callback.
     * @param callback Function to call when button is clicked
     */
    void setOnClick(std::function<void()> callback) { m_on_click = callback; }
    
    /**
     * @brief Sets whether the button tracks mouse globally when pressed.
     * @param global_tracking If true, button continues tracking mouse even outside widget bounds
     */
    void setGlobalMouseTracking(bool global_tracking) { m_global_mouse_tracking = global_tracking; }

private:
    ButtonSymbol m_symbol;
    bool m_pressed;
    bool m_hovered;
    bool m_enabled;
    bool m_global_mouse_tracking;
    std::function<void()> m_on_click;
    
    /**
     * @brief Rebuilds the button surface based on current state.
     */
    void rebuildSurface();
    
    /**
     * @brief Draws the button background and bevel.
     * @param surface Surface to draw on
     * @param pressed Whether button appears pressed
     */
    void drawButtonBackground(Surface& surface, bool pressed);
    
    /**
     * @brief Draws the button symbol.
     * @param surface Surface to draw on
     * @param symbol Symbol to draw
     * @param enabled Whether button is enabled (affects symbol color)
     */
    void drawButtonSymbol(Surface& surface, ButtonSymbol symbol, bool enabled);
};

#endif // BUTTONWIDGET_H