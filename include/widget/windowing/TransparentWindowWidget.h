/*
 * TransparentWindowWidget.h - Transparent floating window without decorations
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

#ifndef TRANSPARENTWINDOWWIDGET_H
#define TRANSPARENTWINDOWWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace Windowing {

// Bring Foundation widgets into this namespace for inheritance
using PsyMP3::Widget::Foundation::DrawableWidget;

/**
 * @brief A transparent floating window without titlebars or borders.
 * 
 * This widget provides a transparent container for UI elements that need
 * to float above the main interface. It supports custom opacity levels,
 * Z-order management, and optional mouse event pass-through.
 * 
 * Key features:
 * - No titlebar or window decorations
 * - Adjustable transparency/opacity
 * - Z-order aware for proper layering
 * - Optional mouse event pass-through (for ToastWidget)
 * - Custom background rendering (solid, gradient, rounded corners, etc.)
 */
class TransparentWindowWidget : public DrawableWidget {
public:
    /**
     * @brief Constructor for TransparentWindowWidget.
     * @param width Window width in pixels
     * @param height Window height in pixels
     * @param opacity Background opacity (0.0 = fully transparent, 1.0 = fully opaque)
     * @param mouse_transparent Whether to pass through mouse events
     */
    TransparentWindowWidget(int width, int height, float opacity = 0.9f, bool mouse_transparent = false);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~TransparentWindowWidget() = default;
    
    /**
     * @brief Sets the Z-order level for this window.
     * @param z_order Z-order level (see ZOrder namespace)
     */
    void setZOrder(int z_order) { m_z_order = z_order; }
    
    /**
     * @brief Gets the current Z-order level.
     * @return Z-order level
     */
    int getZOrder() const { return m_z_order; }
    
    /**
     * @brief Sets the background opacity.
     * @param opacity Opacity level (0.0 = transparent, 1.0 = opaque)
     */
    void setOpacity(float opacity);
    
    /**
     * @brief Gets the current opacity level.
     * @return Opacity level (0.0 to 1.0)
     */
    float getOpacity() const { return m_opacity; }
    
    /**
     * @brief Sets whether this window passes through mouse events.
     * @param transparent true to pass through mouse events, false to handle them
     */
    void setMouseTransparent(bool transparent) { m_mouse_pass_through = transparent; }
    
    /**
     * @brief Checks if this window passes through mouse events.
     * @return true if mouse events pass through, false if handled
     */
    bool isMousePassThrough() const { return m_mouse_pass_through; }
    
    /**
     * @brief Sets the background color (before opacity is applied).
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b);
    
    /**
     * @brief Sets corner radius for rounded rectangle background.
     * @param radius Corner radius in pixels (0 = square corners)
     */
    void setCornerRadius(int radius) { m_corner_radius = radius; invalidate(); }
    
    /**
     * @brief Mouse event handling - respects pass-through setting.
     */
    virtual bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    virtual bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    virtual bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

protected:
    /**
     * @brief Draws the transparent background and any decorations.
     * @param surface The surface to draw on
     */
    virtual void draw(Surface& surface) override;
    
    /**
     * @brief Draws a rounded rectangle on the surface.
     * @param surface Target surface
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param radius Corner radius
     * @param color RGBA color
     */
    virtual void drawRoundedRect(Surface& surface, int x, int y, int width, int height, 
                                int radius, uint32_t color);

private:
    int m_z_order;
    float m_opacity;
    bool m_mouse_pass_through;
    int m_corner_radius;
    
    // Background color components (before alpha is applied)
    uint8_t m_bg_r, m_bg_g, m_bg_b;
};

} // namespace Windowing
} // namespace Widget
} // namespace PsyMP3

#endif // TRANSPARENTWINDOWWIDGET_H