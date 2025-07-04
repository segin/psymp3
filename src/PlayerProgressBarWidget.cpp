/*
 * ProgressBarWidget.cpp - Implementation for progress bar widget
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

PlayerProgressBarWidget::PlayerProgressBarWidget(int width, int height)
    : Widget()
    , m_width(width)
    , m_height(height)
    , m_progress(0.0)
    , m_drag_progress(0.0)
    , m_is_dragging(false)
{
    // Set initial position and size
    Rect pos(0, 0, width, height);
    setPos(pos);
    
    // Build the initial progress bar surface
    rebuildSurface();
}

bool PlayerProgressBarWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT) {
        // Check if click is within the progress bar area
        if (relative_x >= 0 && relative_x < m_width &&
            relative_y >= 0 && relative_y < m_height) {
            
            m_is_dragging = true;
            
            // Convert coordinates to progress value
            double progress = coordinateToProgress(relative_x);
            m_drag_progress = progress;
            
            // Call seek start callback
            if (m_on_seek_start) {
                m_on_seek_start(progress);
            }
            
            // Rebuild surface to show drag progress
            rebuildSurface();
            
            return true;
        }
    }
    
    return false;
}

bool PlayerProgressBarWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (m_is_dragging) {
        // Clamp coordinates to widget bounds
        int clamped_x = relative_x;
        if (clamped_x < 0) clamped_x = 0;
        if (clamped_x >= m_width) clamped_x = m_width - 1;
        
        // Convert coordinates to progress value
        double progress = coordinateToProgress(clamped_x);
        m_drag_progress = progress;
        
        // Call seek update callback
        if (m_on_seek_update) {
            m_on_seek_update(progress);
        }
        
        // Rebuild surface to show updated drag progress
        rebuildSurface();
        
        return true;
    }
    
    return false;
}

bool PlayerProgressBarWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button == SDL_BUTTON_LEFT && m_is_dragging) {
        m_is_dragging = false;
        
        // Finalize the seek
        double final_progress = m_drag_progress;
        
        // Call seek end callback
        if (m_on_seek_end) {
            m_on_seek_end(final_progress);
        }
        
        // Update actual progress to match drag progress
        m_progress = final_progress;
        
        // Rebuild surface to show final state
        rebuildSurface();
        
        return true;
    }
    
    return false;
}

void PlayerProgressBarWidget::setProgress(double progress)
{
    // Clamp progress to valid range
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    if (m_progress != progress) {
        m_progress = progress;
        if (!m_is_dragging) {
            rebuildSurface();
        }
    }
}

void PlayerProgressBarWidget::setDragProgress(double progress)
{
    // Clamp progress to valid range
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    
    if (m_drag_progress != progress) {
        m_drag_progress = progress;
        if (m_is_dragging) {
            rebuildSurface();
        }
    }
}

void PlayerProgressBarWidget::rebuildSurface()
{
    // Create the progress bar surface
    auto progress_surface = std::make_unique<Surface>(m_width, m_height, true);
    
    // Fill with transparent background
    uint32_t transparent = progress_surface->MapRGBA(0, 0, 0, 0);
    progress_surface->FillRect(transparent);
    
    // Frame is drawn by main rendering loop, not by widget
    
    // Draw the rainbow fill inside the frame
    double display_progress = m_is_dragging ? m_drag_progress : m_progress;
    drawRainbowFill(*progress_surface, display_progress);
    
    // Set the surface
    setSurface(std::move(progress_surface));
}

void PlayerProgressBarWidget::drawFrame(Surface& surface) const
{
    uint32_t white = surface.MapRGBA(255, 255, 255, 255);
    
    // Match original frame exactly (relative to widget coordinates)
    // Original was: vline(399,370,385) vline(621,370,385) hline(399,402,370) hline(399,402,385) hline(618,621,370) hline(618,621,385)
    // Widget is positioned at (400,373) with size (220,10), so we need to translate:
    // Left side: x=399 becomes x=-1, x=402 becomes x=2
    // Right side: x=618 becomes x=218, x=621 becomes x=221
    // Y coords: y=370 becomes y=-3, y=385 becomes y=12
    
    // But we need to draw within our widget bounds, so adjust:
    surface.vline(0, 0, m_height - 1, white);                    // Left vertical (x=-1 -> x=0)
    surface.vline(m_width - 1, 0, m_height - 1, white);          // Right vertical (x=221 -> x=219)
    surface.hline(0, 3, 0, white);                               // Left top horizontal (399->402 = 3 pixels wide)
    surface.hline(0, 3, m_height - 1, white);                    // Left bottom horizontal  
    surface.hline(m_width - 4, m_width - 1, 0, white);           // Right top horizontal (618->621 = 3 pixels wide)
    surface.hline(m_width - 4, m_width - 1, m_height - 1, white); // Right bottom horizontal
}

void PlayerProgressBarWidget::drawRainbowFill(Surface& surface, double progress) const
{
    // Widget is positioned at x=400, y=373 within the original frame
    // Original frame inner area is x=400 to x=620 (220 pixels), y=371 to y=384 
    // Widget fills the inner area perfectly: 220x10 at y=373
    double fill_width = progress * m_width; // Fill based on widget width (220 pixels)
    
    // Draw rainbow fill from left edge of widget
    for (int x = 0; x < static_cast<int>(fill_width); x++) {
        // Rainbow color calculation (exact original algorithm)
        uint8_t r, g, b;
        
        if (x > 146) {
            // Zone 3: x > 146
            r = static_cast<uint8_t>((x - 146) * 3.5);
            g = 0;
            b = 255;
        } else if (x < 73) {
            // Zone 1: x < 73  
            r = 128;
            g = 255;
            b = static_cast<uint8_t>(x * 3.5);
        } else {
            // Zone 2: 73 <= x <= 146
            r = static_cast<uint8_t>(128 - ((x - 73) * 1.75));
            g = static_cast<uint8_t>(255 - ((x - 73) * 3.5));
            b = 255;
        }
        
        // Draw vertical line for this x position across full widget height
        surface.vline(x, 0, m_height - 1, r, g, b, 255);
    }
}

double PlayerProgressBarWidget::coordinateToProgress(int x) const
{
    // Convert x coordinate to progress (0.0 to 1.0)
    // Use full 220-pixel width like original
    if (x < 0) return 0.0;
    if (x >= 220) return 1.0;
    
    return static_cast<double>(x) / 220.0;
}