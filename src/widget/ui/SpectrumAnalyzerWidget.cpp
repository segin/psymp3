/*
 * SpectrumAnalyzerWidget.cpp - Implementation for spectrum analyzer widget
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

using Foundation::Widget;
using Foundation::DrawableWidget;

SpectrumAnalyzerWidget::SpectrumAnalyzerWidget(int width, int height)
    : DrawableWidget(width, height)
    , m_visualization_mode(0)
    , m_color_scheme(0)
    , m_decay_factor(1.0f) // Default decay factor where 1.0 = 63 alpha
    , m_scale_factor(2) // Default scale factor
{
    // Initialize with empty spectrum data
    m_spectrum_data.resize(64, 0.0f); // Default to 64 bands
}

void SpectrumAnalyzerWidget::updateSpectrum(const float* spectrum_data, int num_bands, int scale_factor, float decay_factor)
{
    if (spectrum_data && num_bands > 0) {
        // Update live values from Player
        m_scale_factor = scale_factor;
        m_decay_factor = decay_factor;
        
        m_spectrum_data.resize(num_bands);
        float max_val = 0.0f;
        bool data_changed = false;
        
        for (int i = 0; i < num_bands; ++i) {
            // Apply gain and logarithmic scaling exactly like the original renderSpectrum
            float gained_amplitude = spectrum_data[i] * 5.0f; // Same 5x gain as original
            // Use the actual Util::logarithmicScale function with live scale_factor
            float scaled_amplitude = Util::logarithmicScale(m_scale_factor, gained_amplitude);
            
            if (std::abs(scaled_amplitude - m_spectrum_data[i]) > 0.001f) {
                data_changed = true;
            }
            m_spectrum_data[i] = scaled_amplitude;
            max_val = std::max(max_val, m_spectrum_data[i]);
        }
        
        // Only redraw if data actually changed
        if (data_changed) {
            invalidate(); // Trigger redraw
        }
    }
}

void SpectrumAnalyzerWidget::setVisualizationMode(int mode)
{
    if (m_visualization_mode != mode) {
        m_visualization_mode = mode;
        invalidate();
    }
}

void SpectrumAnalyzerWidget::setColorScheme(int color_scheme)
{
    if (m_color_scheme != color_scheme) {
        m_color_scheme = color_scheme;
        invalidate();
    }
}

void SpectrumAnalyzerWidget::setDecayFactor(float decay_factor)
{
    m_decay_factor = decay_factor;
}

void SpectrumAnalyzerWidget::draw(Surface& surface)
{
    // Maintain a persistent spectrum surface that accumulates frames over time
    static std::unique_ptr<Surface> spectrum_surface_ptr; // Persistent across calls
    static std::unique_ptr<Surface> fade_surface_ptr; // For fade effect
    static uint8_t cached_fade_alpha = 255; // Cache to avoid redundant SetAlpha calls
    
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    // Create persistent spectrum surface once and reuse
    if (!spectrum_surface_ptr || spectrum_surface_ptr->width() != width || spectrum_surface_ptr->height() != height) {
        spectrum_surface_ptr = std::make_unique<Surface>(width, height);
        spectrum_surface_ptr->FillRect(spectrum_surface_ptr->MapRGB(0, 0, 0)); // Start with black
    }
    Surface& spectrum_surface = *spectrum_surface_ptr;
    
    // Create fade surface once and reuse (matches original algorithm)
    if (!fade_surface_ptr || fade_surface_ptr->width() != width || fade_surface_ptr->height() != height) {
        fade_surface_ptr = std::make_unique<Surface>(width, height);
        fade_surface_ptr->FillRect(fade_surface_ptr->MapRGB(0, 0, 0));
        cached_fade_alpha = 255; // Reset cache when surface is recreated
    }
    Surface& fade_surface = *fade_surface_ptr;
    
    // Calculate alpha for fade (0-255). Original: decayfactor from 0.5 to 2.0
    // Original formula: (255 * (decayfactor / 4.0f))  where decayfactor=1.0 gives 63 alpha
    uint8_t fade_alpha = static_cast<uint8_t>(255 * (m_decay_factor / 4.0f));
    
    // Only call SetAlpha if the fade_alpha has changed to avoid redundant SDL calls
    if (fade_alpha != cached_fade_alpha) {
        fade_surface.SetAlpha(SDL_SRCALPHA, fade_alpha);
        cached_fade_alpha = fade_alpha;
    }
    
    // Apply fade effect to the persistent spectrum surface (darkens previous content)
    Rect blit_rect(0, 0, width, height);
    spectrum_surface.Blit(fade_surface, blit_rect);
    
    // Draw new spectrum bars directly onto the persistent spectrum surface
    switch (m_visualization_mode) {
        case 0:
            drawBars(spectrum_surface);
            break;
        case 1:
            drawOscilloscope(spectrum_surface);
            break;
        default:
            drawBars(spectrum_surface);
            break;
    }
    
    // Copy the persistent spectrum surface to the output surface
    surface.Blit(spectrum_surface, blit_rect);
}

void SpectrumAnalyzerWidget::drawBars(Surface& surface)
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    if (m_spectrum_data.empty() || width <= 0 || height <= 0) {
        return;
    }
    
    int num_bands = m_spectrum_data.size();
    int bar_width = std::max(1, width / num_bands);
    int spacing = std::max(0, (width - (bar_width * num_bands)) / (num_bands + 1));
    
    // Find max value for debug
    float max_val = 0.0f;
    for (const float& val : m_spectrum_data) {
        max_val = std::max(max_val, val);
    }
    
    
    for (int i = 0; i < num_bands; ++i) {
        float value = m_spectrum_data[i];
        // Calculate y position like the original: from amplitude down to bottom
        int y_start = static_cast<int>(height - (value * height));
        y_start = std::max(0, std::min(height - 1, y_start)); // Clamp to valid range
        
        int x = spacing + i * (bar_width + spacing);
        
        // Get color components directly (like the old code did)
        uint8_t r, g, b;
        if (i > 213) {
            // Zone 3: x > 213
            r = static_cast<uint8_t>((i - 214) * 2.4);
            g = 0;
            b = 255;
        } else if (i < 106) {
            // Zone 1: x < 106
            r = 128;
            g = 255;
            b = static_cast<uint8_t>(i * 2.398);
        } else {
            // Zone 2: 106 <= x <= 213
            r = static_cast<uint8_t>(128 - ((i - 106) * 1.1962615));
            g = static_cast<uint8_t>(255 - ((i - 106) * 2.383177));
            b = 255;
        }
        
        // Draw rectangle from y_start down to bottom (like original rectangle call)
        surface.box(x, y_start, x + bar_width - 1, height - 1, r, g, b, 255);
    }
}

void SpectrumAnalyzerWidget::drawOscilloscope(Surface& surface)
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    if (m_spectrum_data.empty() || width <= 0 || height <= 0) {
        return;
    }
    
    int num_points = std::min(static_cast<int>(m_spectrum_data.size()), width);
    int center_y = height / 2;
    int amplitude = height / 4;
    
    // Draw oscilloscope line
    for (int x = 0; x < num_points - 1; ++x) {
        int x1 = x * width / num_points;
        int x2 = (x + 1) * width / num_points;
        
        float value1 = (m_spectrum_data[x] - 0.5f) * 2.0f; // Convert to -1 to +1 range
        float value2 = (m_spectrum_data[x + 1] - 0.5f) * 2.0f;
        
        int y1 = center_y - static_cast<int>(value1 * amplitude);
        int y2 = center_y - static_cast<int>(value2 * amplitude);
        
        // Clamp y coordinates
        y1 = std::max(0, std::min(height - 1, y1));
        y2 = std::max(0, std::min(height - 1, y2));
        
        uint32_t color = getSpectrumColor(value1, x, surface); // Color based on position, not amplitude
        surface.line(x1, y1, x2, y2, 
                    (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
    }
}

uint32_t SpectrumAnalyzerWidget::getSpectrumColor(float value, int position, Surface& surface)
{
    // FIXED: Color should be based on POSITION (frequency), not amplitude value
    // This matches the original renderSpectrum behavior with precomputed colors
    
    switch (m_color_scheme) {
        case 0: // Original spectrum colors (exact old renderSpectrum algorithm)
            {
                // Use position directly (should be 0-319 for 320 bands)
                int x = position;
                uint8_t r, g, b;
                
                if (x > 213) {
                    // Zone 3: x > 213
                    r = static_cast<uint8_t>((x - 214) * 2.4);
                    g = 0;
                    b = 255;
                } else if (x < 106) {
                    // Zone 1: x < 106
                    r = 128;
                    g = 255;
                    b = static_cast<uint8_t>(x * 2.398);
                } else {
                    // Zone 2: 106 <= x <= 213
                    r = static_cast<uint8_t>(128 - ((x - 106) * 1.1962615));
                    g = static_cast<uint8_t>(255 - ((x - 106) * 2.383177));
                    b = 255;
                }
                return surface.MapRGB(r, g, b);
            }
        case 1: // Simple position-based rainbow (not amplitude-based)
            {
                float hue = static_cast<float>(position) / m_spectrum_data.size() * 6.0f;
                int h = static_cast<int>(hue) % 6;
                float f = hue - h;
                
                // Full intensity colors (not based on amplitude)
                uint8_t v = 255;
                uint8_t p = 0;
                uint8_t q = static_cast<uint8_t>((1.0f - f) * 255);
                uint8_t t = static_cast<uint8_t>(f * 255);
                
                switch (h) {
                    case 0: return surface.MapRGB(v, t, p);
                    case 1: return surface.MapRGB(q, v, p);
                    case 2: return surface.MapRGB(p, v, t);
                    case 3: return surface.MapRGB(p, q, v);
                    case 4: return surface.MapRGB(t, p, v);
                    case 5: return surface.MapRGB(v, p, q);
                    default: return surface.MapRGB(v, v, v);
                }
            }
        case 2: // Solid green (classic spectrum analyzer)
            return surface.MapRGB(0, 255, 0);
        default:
            return surface.MapRGB(255, 255, 255); // White
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3