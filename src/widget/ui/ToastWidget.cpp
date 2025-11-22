/*
 * ToastWidget.cpp - Implementation for Android-style toast notifications
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

#include "psymp3.h"

namespace PsyMP3 {
namespace Widget {
namespace UI {

ToastWidget::ToastWidget(const std::string& message, Font* font, int duration_ms)
    : TransparentWindowWidget(100, 50, 0.85f, true)  // Start with default size, 85% opacity, mouse-transparent
    , m_message(message)
    , m_font(font)
    , m_duration_ms(duration_ms)
    , m_start_time(std::chrono::steady_clock::now())
{
    // Set Z-order to maximum (always on top)
    setZOrder(ZOrder::MAX);
    
    // Set dark background color to match original ToastNotification (ignore alpha, focus on RGB)
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

bool ToastWidget::shouldDismiss() const
{
    if (m_duration_ms <= 0) {
        return false; // No auto-dismiss
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    return elapsed.count() >= m_duration_ms;
}

void ToastWidget::resetTimer()
{
    m_start_time = std::chrono::steady_clock::now();
}

void ToastWidget::draw(Surface& surface)
{
    // Step 1: Render the Label onto its Surface and get dimensions
    std::unique_ptr<Surface> label_surface;
    int label_width = 0, label_height = 0;
    
    if (m_font && !m_message.empty()) {
        label_surface = m_font->Render(m_message, 255, 255, 255); // White text
        if (label_surface && label_surface->isValid()) {
            label_width = label_surface->width();
            label_height = label_surface->height();
        }
    }
    
    // Step 2: Use actual surface dimensions (should match calculateSize result)
    int window_width = surface.width();
    int window_height = surface.height();
    
    // Step 3: Clear surface to fully transparent
    surface.FillRect(surface.MapRGBA(0, 0, 0, 0));
    
    const int corner_radius = 8;
    
    // Step 4: Draw outer border background with original ToastNotification colors
    drawSimpleRoundedRect(surface, 0, 0, window_width, window_height, corner_radius, 100, 100, 100, 255);
    
    // Step 5: Draw inner background area (2px smaller, offset by 1px) with original colors
    if (window_width > 4 && window_height > 4) { // Ensure we have space for inner rect
        drawSimpleRoundedRect(surface, 1, 1, window_width - 2, window_height - 2, corner_radius - 1, 50, 50, 50, 255);
    }
    
    // Step 6: Apply 85% opacity while preserving transparent areas
    applyRelativeOpacity(surface, 0.85f);
    
    // Step 7: Render the Label at (8, 8)
    if (label_surface && label_surface->isValid()) {
        Rect label_rect(8, 8, label_width, label_height);
        surface.Blit(*label_surface, label_rect);
    }
}

Rect ToastWidget::calculateSize(const std::string& message, Font* font, int padding)
{
    if (!font || message.empty()) {
        return Rect(0, 0, 100, 50); // Minimum default size
    }
    
    // Render the text to get its dimensions
    auto text_surface = font->Render(message, 255, 255, 255);
    
    if (!text_surface || !text_surface->isValid()) {
        return Rect(0, 0, 100, 50); // Fallback size
    }
    
    // Add padding around the text
    int width = text_surface->width() + (padding * 2);
    int height = text_surface->height() + (padding * 2);
    
    // Ensure minimum size
    width = std::max(width, 100);
    height = std::max(height, 50);
    
    // Ensure maximum reasonable size (don't let toasts get too big)
    width = std::min(width, 400);
    height = std::min(height, 100);
    
    return Rect(0, 0, width, height);
}

void ToastWidget::updateSize()
{
    // Calculate size based on actual label dimensions + 16px padding
    int label_width = 0, label_height = 0;
    
    if (m_font && !m_message.empty()) {
        auto label_surface = m_font->Render(m_message, 255, 255, 255);
        if (label_surface && label_surface->isValid()) {
            label_width = label_surface->width();
            label_height = label_surface->height();
        }
    }
    
    // Add 16px to both axes as specified
    int window_width = label_width + 16;
    int window_height = label_height + 16;
    
    // Ensure minimum size
    window_width = std::max(window_width, 50);
    window_height = std::max(window_height, 30);
    
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

void ToastWidget::applyOpacity(Surface& surface, float opacity)
{
    // Apply opacity to all pixels in the surface using SDL surface access
    SDL_Surface* sdl_surface = surface.getHandle();
    if (!sdl_surface || !sdl_surface->pixels) return;
    
    SDL_LockSurface(sdl_surface);
    
    uint32_t* pixels = static_cast<uint32_t*>(sdl_surface->pixels);
    int width = surface.width();
    int height = surface.height();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t& pixel = pixels[y * width + x];
            
            // Extract RGBA components
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, sdl_surface->format, &r, &g, &b, &a);
            
            // Apply opacity to alpha channel
            a = static_cast<uint8_t>(a * opacity);
            
            // Reassemble pixel
            pixel = SDL_MapRGBA(sdl_surface->format, r, g, b, a);
        }
    }
    
    SDL_UnlockSurface(sdl_surface);
}

void ToastWidget::applyRelativeOpacity(Surface& surface, float opacity)
{
    // Apply relative opacity - only affects pixels that are already non-transparent
    SDL_Surface* sdl_surface = surface.getHandle();
    if (!sdl_surface || !sdl_surface->pixels) return;
    
    SDL_LockSurface(sdl_surface);
    
    uint32_t* pixels = static_cast<uint32_t*>(sdl_surface->pixels);
    int width = surface.width();
    int height = surface.height();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t& pixel = pixels[y * width + x];
            
            // Extract RGBA components
            uint8_t r, g, b, a;
            SDL_GetRGBA(pixel, sdl_surface->format, &r, &g, &b, &a);
            
            // Only apply opacity if pixel is not already fully transparent
            if (a > 0) {
                a = static_cast<uint8_t>(a * opacity);
            }
            
            // Reassemble pixel
            pixel = SDL_MapRGBA(sdl_surface->format, r, g, b, a);
        }
    }
    
    SDL_UnlockSurface(sdl_surface);
}
} /
/ namespace UI
} // namespace Widget
} // namespace PsyMP3
