/*
 * ToastWidget.cpp - Implementation for Android-style toast notifications
 * This file is part of PsyMP3.
 * Copyright © 2025-2026 Kirn Gill <segin2005@gmail.com>
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
#include <sstream>

namespace PsyMP3 {
namespace Widget {
namespace UI {

using Foundation::Widget;
using Foundation::DrawableWidget;

ToastWidget::ToastWidget(const std::string& message, Font* font, int duration_ms)
    : TransparentWindowWidget(100, 50, 0.85f, true)  // Start with default size, 85% opacity, mouse-transparent
    , m_message(message)
    , m_font(font)
    , m_duration_ms(duration_ms)
    , m_start_time(std::chrono::steady_clock::now())
    , m_exit_start_time()
    , m_exit_duration_ms(FADE_OUT_MS)
    , m_exiting(false)
{
    // Keep toast notifications in the reserved always-on-top band while
    // leaving the absolute maximum slot available for future system overlays.
    setZOrder(ZOrder::TOAST);
    
    // Set dark background color to match original Android Toast notification style (ignore alpha, focus on RGB)
    setBackgroundColor(50, 50, 50);  // Original inner background color
    
    // Enable rounded corners
    setCornerRadius(DEFAULT_CORNER_RADIUS);
    
    // Calculate and set appropriate size
    updateSize();
}

void ToastWidget::setMessage(const std::string& message)
{
    if (m_message != message) {
        m_message = message;
        updateSize();
        invalidate(); // Trigger redraw
    }
}

void ToastWidget::dismiss()
{
    if (m_on_dismiss) {
        m_on_dismiss(this);
    }
}

void ToastWidget::beginDismiss(int fade_duration_ms)
{
    if (m_exiting) {
        if (fade_duration_ms > 0 && fade_duration_ms < m_exit_duration_ms) {
            m_exit_duration_ms = fade_duration_ms;
        }
        return;
    }

    m_exiting = true;
    m_exit_duration_ms = std::max(1, fade_duration_ms);
    m_exit_start_time = std::chrono::steady_clock::now();
}

bool ToastWidget::shouldDismiss() const
{
    if (m_duration_ms <= 0 || m_exiting) {
        return false; // No auto-dismiss
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    return elapsed.count() >= m_duration_ms;
}

bool ToastWidget::isFinished() const
{
    if (!m_exiting) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_exit_start_time);
    return elapsed.count() >= m_exit_duration_ms;
}

void ToastWidget::BlitTo(Surface& target)
{
    if (isAnimationActive()) {
        invalidate();
    }
    TransparentWindowWidget::BlitTo(target);
}

void ToastWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    if (isAnimationActive()) {
        invalidate();
    }
    TransparentWindowWidget::recursiveBlitTo(target, parent_absolute_pos);
}

void ToastWidget::resetTimer()
{
    m_start_time = std::chrono::steady_clock::now();
}

void ToastWidget::draw(Surface& surface)
{
    // Step 1: Use actual surface dimensions (should match calculateSize result)
    int window_width = surface.width();
    int window_height = surface.height();

    if (surface.getHandle()) {
        SDL_SetSurfaceBlendMode(surface.getHandle(), SDL_BLENDMODE_BLEND);
    }
    
    // Step 2: Clear surface to fully transparent
    surface.FillRect(surface.MapRGBA(0, 0, 0, 0));
    
    const int corner_radius = 8;
    
    // Step 3: Draw outer border background with original Toast style colors
    drawSimpleRoundedRect(surface, 0, 0, window_width, window_height, corner_radius, 100, 100, 100, 255);
    
    // Step 4: Draw inner background area (2px smaller, offset by 1px) with original colors
    if (window_width > 4 && window_height > 4) { // Ensure we have space for inner rect
        drawSimpleRoundedRect(surface, 1, 1, window_width - 2, window_height - 2, corner_radius - 1, 50, 50, 50, 255);
    }
    
    // Step 5: Render the Label (handling multiline)
    std::stringstream ss(m_message);
    std::string line;
    int y_offset = 8;
    while (std::getline(ss, line, '\n')) {
        if (m_font && !line.empty()) {
            auto line_surface = m_font->RenderLCD(TagLib::String(line, TagLib::String::UTF8), 255, 255, 255, 50, 50, 50);
            if (line_surface && line_surface->isValid()) {
                Rect label_rect(8, y_offset, line_surface->width(), line_surface->height());
                surface.Blit(*line_surface, label_rect);
                y_offset += line_surface->height();
            }
        } else {
             // For empty lines, add some spacing if font is available
             // Assuming ~12px for standard font if not easily accessible
             y_offset += 12;
        }
    }

    // Step 6: Apply animation-scaled opacity to the complete toast so the
    // shell and text fade together.
    const float animation_opacity = std::clamp(currentAnimationOpacity(), 0.0f, 1.0f);
    surface.applyRelativeOpacity(0.85f * animation_opacity);
}

::Rect ToastWidget::calculateSize(const std::string& message, ::Font* font, int padding)
{
    if (!font || message.empty()) {
        return Rect(0, 0, 100, 50); // Minimum default size
    }
    
    int max_width = 0;
    int total_height = 0;
    
    std::stringstream ss(message);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (!line.empty()) {
            auto text_surface = font->RenderLCD(TagLib::String(line, TagLib::String::UTF8), 255, 255, 255, 50, 50, 50);
            if (text_surface && text_surface->isValid()) {
                max_width = std::max(max_width, static_cast<int>(text_surface->width()));
                total_height += text_surface->height();
            }
        } else {
             total_height += 12; // Approximation for empty line
        }
    }
    
    // Add padding around the text
    int width = max_width + (padding * 2);
    int height = total_height + (padding * 2);
    
    // Ensure minimum size
    width = std::max(width, 100);
    height = std::max(height, 50);
    
    // Ensure maximum reasonable size (don't let toasts get too big)
    width = std::min(width, 600); // Increased max width for errors
    height = std::min(height, 300); // Increased max height
    
    return Rect(0, 0, width, height);
}

void ToastWidget::updateSize()
{
    // Calculate size based on actual label dimensions + 16px padding
    int max_width = 0;
    int total_height = 0;
    
    if (m_font && !m_message.empty()) {
        std::stringstream ss(m_message);
        std::string line;
        while (std::getline(ss, line, '\n')) {
            if (!line.empty()) {
                auto label_surface = m_font->RenderLCD(TagLib::String(line, TagLib::String::UTF8), 255, 255, 255, 50, 50, 50);
                if (label_surface && label_surface->isValid()) {
                    max_width = std::max(max_width, static_cast<int>(label_surface->width()));
                    total_height += label_surface->height();
                }
            } else {
                 total_height += 12; // Approximation
            }
        }
    }
    
    // Add 16px to both axes as specified
    int window_width = max_width + 16;
    int window_height = total_height + 16;
    
    // Ensure minimum size
    window_width = std::max(window_width, 50);
    window_height = std::max(window_height, 30);

    // Clamp to a sane maximum so long error messages/paths don't render wider
    // than the 640px canvas (which would push the centered toast off-screen at
    // a negative x). The toast has no word-wrap, so an over-long single line is
    // truncated at the right edge by the blit clip. 600px matches the intent of
    // the (dead) calculateSize() clamp.
    window_width = std::min(window_width, 600);
    window_height = std::min(window_height, 300);

    // Update our position to maintain the new size
    Rect current_pos = getPos();
    current_pos.width(window_width);
    current_pos.height(window_height);
    setPos(current_pos);
    
    // Trigger a redraw since size changed
    onResize(window_width, window_height);
}

void ToastWidget::drawRoundedRect(Surface& surface, int x, int y, int width, int height, int radius, uint32_t color)
{
    // Extract RGBA components from packed color
    uint8_t r = (color >> 24) & 0xFF;
    uint8_t g = (color >> 16) & 0xFF;
    uint8_t b = (color >> 8) & 0xFF;
    uint8_t a = color & 0xFF;
    
    drawRoundedRectRGBA(surface, x, y, width, height, radius, r, g, b, a);
}

void ToastWidget::drawRoundedRectRGBA(Surface& surface, int x, int y, int width, int height, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    // Use the simple version for now
    drawSimpleRoundedRect(surface, x, y, width, height, radius, r, g, b, a);
}

void ToastWidget::drawSimpleRoundedRect(Surface& surface, int x, int y, int width, int height, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    // Clamp radius to prevent artifacts
    int max_radius = std::min(width / 2, height / 2);
    radius = std::min(radius, max_radius);
    
    if (radius <= 0) {
        // Just draw a regular rectangle
        surface.box(x, y, x + width - 1, y + height - 1, r, g, b, a);
        return;
    }
    
    // Draw the main rectangle body (center area)
    if (height > 2 * radius && width > 2 * radius) {
        surface.box(x + radius, y, x + width - radius - 1, y + height - 1, r, g, b, a); // Horizontal center
        surface.box(x, y + radius, x + radius - 1, y + height - radius - 1, r, g, b, a); // Left vertical
        surface.box(x + width - radius, y + radius, x + width - 1, y + height - radius - 1, r, g, b, a); // Right vertical
    }
    
    // Draw rounded corners using simpler algorithm
    drawRoundedCorner(surface, x + radius, y + radius, radius, r, g, b, a, 0); // Top-left
    drawRoundedCorner(surface, x + width - radius - 1, y + radius, radius, r, g, b, a, 1); // Top-right  
    drawRoundedCorner(surface, x + radius, y + height - radius - 1, radius, r, g, b, a, 2); // Bottom-left
    drawRoundedCorner(surface, x + width - radius - 1, y + height - radius - 1, radius, r, g, b, a, 3); // Bottom-right
}

void ToastWidget::drawRoundedCorner(Surface& surface, int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int corner)
{
    // Draw filled circle quadrant for each corner
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                int px = cx, py = cy;
                bool draw = false;
                
                // Map to correct quadrant based on corner
                switch (corner) {
                    case 0: // Top-left
                        if (dx <= 0 && dy <= 0) { px = cx + dx; py = cy + dy; draw = true; }
                        break;
                    case 1: // Top-right
                        if (dx >= 0 && dy <= 0) { px = cx + dx; py = cy + dy; draw = true; }
                        break;
                    case 2: // Bottom-left
                        if (dx <= 0 && dy >= 0) { px = cx + dx; py = cy + dy; draw = true; }
                        break;
                    case 3: // Bottom-right
                        if (dx >= 0 && dy >= 0) { px = cx + dx; py = cy + dy; draw = true; }
                        break;
                }
                
                if (draw && px >= 0 && px < surface.width() && py >= 0 && py < surface.height()) {
                    surface.pixel(px, py, r, g, b, a);
                }
            }
        }
    }
}

void ToastWidget::drawFilledCircleQuadrant(Surface& surface, int cx, int cy, int radius, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int quadrant)
{
    // Use Bresenham-like algorithm for filled circle quadrant
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                int px = cx, py = cy;
                
                // Apply quadrant transformation
                switch (quadrant) {
                    case 0: // Bottom-right
                        if (x >= 0 && y >= 0) { px = cx + x; py = cy + y; }
                        else continue;
                        break;
                    case 1: // Bottom-left  
                        if (x <= 0 && y >= 0) { px = cx + x; py = cy + y; }
                        else continue;
                        break;
                    case 2: // Top-left
                        if (x <= 0 && y <= 0) { px = cx + x; py = cy + y; }
                        else continue;
                        break;
                    case 3: // Top-right
                        if (x >= 0 && y <= 0) { px = cx + x; py = cy + y; }
                        else continue;
                        break;
                }
                
                // Check bounds and draw pixel
                if (px >= 0 && px < static_cast<int>(surface.width()) && 
                    py >= 0 && py < static_cast<int>(surface.height())) {
                    surface.pixel(px, py, r, g, b, a);
                }
            }
        }
    }
}

float ToastWidget::currentAnimationOpacity() const
{
    const auto now = std::chrono::steady_clock::now();
    const auto enter_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();

    float enter_factor = 1.0f;
    if (FADE_IN_MS > 0) {
        enter_factor = std::clamp(static_cast<float>(enter_elapsed) / static_cast<float>(FADE_IN_MS), 0.0f, 1.0f);
    }

    float exit_factor = 1.0f;
    if (m_exiting) {
        const auto exit_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_exit_start_time).count();
        const float progress = static_cast<float>(exit_elapsed) / static_cast<float>(std::max(1, m_exit_duration_ms));
        exit_factor = 1.0f - std::clamp(progress, 0.0f, 1.0f);
    }

    return enter_factor * exit_factor;
}

bool ToastWidget::isAnimationActive() const
{
    const auto now = std::chrono::steady_clock::now();
    const auto enter_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time).count();
    const bool entering = enter_elapsed < FADE_IN_MS;
    const bool exiting = m_exiting && !isFinished();
    return entering || exiting;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
