/*
 * ToastWidget.h - Android-style toast notification widget
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

#ifndef TOASTWIDGET_H
#define TOASTWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// Bring Windowing widgets into this namespace for inheritance
using PsyMP3::Widget::Windowing::TransparentWindowWidget;

/**
 * @brief Android-style toast notification widget.
 * 
 * ToastWidget displays temporary messages that automatically disappear
 * after a specified duration. It's always mouse-transparent (events pass through)
 * and appears at the top of the Z-order.
 * 
 * Features:
 * - Auto-dismiss after configurable timeout
 * - Mouse-transparent (all events pass through)
 * - Rounded corners with semi-transparent background
 * - Center-aligned text with proper padding
 * - Callback support for dismiss events
 */
class ToastWidget : public TransparentWindowWidget {
public:
    /// Toast duration constants (in milliseconds)
    static constexpr int DURATION_SHORT = 2000;   // 2 seconds
    static constexpr int DURATION_LONG = 3500;    // 3.5 seconds
    
    /**
     * @brief Constructor for ToastWidget.
     * @param message Text message to display
     * @param font Font to use for text rendering
     * @param duration_ms Auto-dismiss timeout in milliseconds
     */
    ToastWidget(const std::string& message, ::Font* font, int duration_ms = DURATION_SHORT);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~ToastWidget() = default;
    
    /**
     * @brief Sets the toast message text.
     * @param message New message text
     */
    void setMessage(const std::string& message);
    
    /**
     * @brief Gets the current message text.
     * @return Current message
     */
    const std::string& getMessage() const { return m_message; }
    
    /**
     * @brief Sets the auto-dismiss duration.
     * @param duration_ms Duration in milliseconds (0 = no auto-dismiss)
     */
    void setDuration(int duration_ms) { m_duration_ms = duration_ms; }
    
    /**
     * @brief Gets the auto-dismiss duration.
     * @return Duration in milliseconds
     */
    int getDuration() const { return m_duration_ms; }
    
    /**
     * @brief Sets a callback to be called when the toast is dismissed.
     * @param callback Function to call on dismiss (takes ToastWidget* as parameter)
     */
    void setOnDismiss(std::function<void(ToastWidget*)> callback) { m_on_dismiss = callback; }
    
    /**
     * @brief Manually dismisses the toast (calls dismiss callback).
     */
    void dismiss();
    
    /**
     * @brief Checks if the toast should be automatically dismissed.
     * Call this regularly from the main loop to handle auto-dismiss.
     * @return true if toast should be dismissed, false otherwise
     */
    bool shouldDismiss() const;
    
    /**
     * @brief Resets the dismiss timer (extends the toast lifetime).
     */
    void resetTimer();

protected:
    /**
     * @brief Draws the toast background and text.
     * @param surface Surface to draw on
     */
    virtual void draw(Surface& surface) override;
    
    /**
     * @brief Calculates the required size for the given message and font.
     * @param message Text to measure
     * @param font Font to use for measurement
     * @param padding Additional padding around text
     * @return Required size as Rect (width and height only)
     */
    static Rect calculateSize(const std::string& message, ::Font* font, int padding = 16);

private:
    std::string m_message;
    ::Font* m_font;
    int m_duration_ms;
    std::chrono::steady_clock::time_point m_start_time;
    std::function<void(ToastWidget*)> m_on_dismiss;
    
    static constexpr int DEFAULT_PADDING = 16;  // Padding around text
    static constexpr int DEFAULT_CORNER_RADIUS = 8;  // Rounded corner radius
    
    /**
     * @brief Updates the widget size based on current message and font.
     */
    void updateSize();
    
    /**
     * @brief Override parent's drawRoundedRect method.
     * @param surface Surface to draw on
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner  
     * @param width Width of rectangle
     * @param height Height of rectangle
     * @param radius Corner radius for rounding
     * @param color RGBA color packed into uint32_t
     */
    virtual void drawRoundedRect(Surface& surface, int x, int y, int width, int height, int radius, uint32_t color) override;
    
    /**
     * @brief Draws a filled rounded rectangle with separate RGBA components.
     * @param surface Surface to draw on
     * @param x X coordinate of top-left corner
     * @param y Y coordinate of top-left corner  
     * @param width Width of rectangle
     * @param height Height of rectangle
     * @param radius Corner radius for rounding
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param a Alpha component (0-255)
     */
    void drawRoundedRectRGBA(Surface& surface, int x, int y, int width, int height, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * @brief Simpler rounded rectangle implementation to avoid artifacts.
     * @param surface Surface to draw on
     * @param x X coordinate
     * @param y Y coordinate
     * @param width Rectangle width
     * @param height Rectangle height
     * @param radius Corner radius
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param a Alpha component (0-255)
     */
    void drawSimpleRoundedRect(Surface& surface, int x, int y, int width, int height, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    
    /**
     * @brief Draws a single rounded corner.
     * @param surface Surface to draw on
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Corner radius
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     * @param a Alpha component (0-255)
     * @param corner Corner position (0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right)
     */
    void drawRoundedCorner(Surface& surface, int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int corner);
    
    /**
     * @brief Draws a filled circle quadrant using circle drawing algorithm.
     * @param surface Surface to draw on
     * @param cx Center X coordinate
     * @param cy Center Y coordinate
     * @param radius Circle radius
     * @param r Red component (0-255)
     * @param g Green component (0-255) 
     * @param b Blue component (0-255)
     * @param a Alpha component (0-255)
     * @param quadrant Quadrant to draw (0=bottom-right, 1=bottom-left, 2=top-left, 3=top-right)
     */
    void drawFilledCircleQuadrant(Surface& surface, int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int quadrant);
    
    /**
     * @brief Applies opacity to all pixels in the surface.
     * @param surface Surface to modify
     * @param opacity Opacity factor (0.0 = transparent, 1.0 = opaque)
     */
    void applyOpacity(Surface& surface, float opacity);
    
    /**
     * @brief Applies relative opacity - only affects pixels that are already non-transparent.
     * Fully transparent pixels (alpha=0) remain fully transparent.
     * @param surface Surface to modify
     * @param opacity Opacity factor (0.0 = transparent, 1.0 = opaque)
     */
    void applyRelativeOpacity(Surface& surface, float opacity);
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // TOASTWIDGET_H