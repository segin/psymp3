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
    
    // Draw the frame (six lines)
    drawFrame(*progress_surface);
    
    // Draw the rainbow fill
    double display_progress = m_is_dragging ? m_drag_progress : m_progress;
    drawRainbowFill(*progress_surface, display_progress);
    
    // Set the surface
    setSurface(std::move(progress_surface));
}

void PlayerProgressBarWidget::drawFrame(Surface& surface) const
{
    uint32_t white = surface.MapRGBA(255, 255, 255, 255);
    
    // Based on original code: six lines total (3 on each side)
    // Left side frame (3 lines)
    surface.vline(0, 0, m_height - 1, white);                    // Left vertical line
    surface.hline(0, 2, 0, white);                               // Left top horizontal line
    surface.hline(0, 2, m_height - 1, white);                    // Left bottom horizontal line
    
    // Right side frame (3 lines)
    surface.vline(m_width - 1, 0, m_height - 1, white);          // Right vertical line
    surface.hline(m_width - 3, m_width - 1, 0, white);           // Right top horizontal line
    surface.hline(m_width - 3, m_width - 1, m_height - 1, white); // Right bottom horizontal line
}

void PlayerProgressBarWidget::drawRainbowFill(Surface& surface, double progress) const
{
    // Calculate fill width (minus frame areas)
    int fill_area_width = m_width - 6; // 3 pixels frame on each side
    double fill_width = progress * fill_area_width;
    
    // Based on original rainbow algorithm
    for (int x = 0; x < static_cast<int>(fill_width); x++) {
        int screen_x = x + 3; // Offset by left frame width
        
        // Y coordinates for the fill area (inner area between frame lines)
        int fill_start_y = 3;
        int fill_end_y = m_height - 4;
        
        // Rainbow color calculation (adapted from original)
        uint8_t r, g, b;
        
        if (x > (fill_area_width * 2 / 3)) {
            // Zone 3: Blue to Magenta transition
            int zone_progress = x - (fill_area_width * 2 / 3);
            int zone_width = fill_area_width / 3;
            r = static_cast<uint8_t>((zone_progress * 255) / zone_width);
            g = 0;
            b = 255;
        } else if (x < (fill_area_width / 3)) {
            // Zone 1: Cyan to Green transition
            int zone_progress = x;
            int zone_width = fill_area_width / 3;
            r = 128;
            g = 255;
            b = static_cast<uint8_t>(255 - (zone_progress * 255) / zone_width);
        } else {
            // Zone 2: Green to Blue transition
            int zone_progress = x - (fill_area_width / 3);
            int zone_width = fill_area_width / 3;
            r = static_cast<uint8_t>(128 - (zone_progress * 128) / zone_width);
            g = static_cast<uint8_t>(255 - (zone_progress * 255) / zone_width);
            b = 255;
        }
        
        // Draw vertical line for this x position
        surface.vline(screen_x, fill_start_y, fill_end_y, r, g, b, 255);
    }
}

double PlayerProgressBarWidget::coordinateToProgress(int x) const
{
    // Convert x coordinate to progress (0.0 to 1.0)
    // Account for frame areas (3 pixels on each side)
    int fill_area_width = m_width - 6;
    int relative_x = x - 3; // Subtract left frame width
    
    if (relative_x < 0) return 0.0;
    if (relative_x >= fill_area_width) return 1.0;
    
    return static_cast<double>(relative_x) / static_cast<double>(fill_area_width);
}