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

SpectrumAnalyzerWidget::SpectrumAnalyzerWidget(int width, int height)
    : DrawableWidget(width, height)
    , m_visualization_mode(0)
    , m_color_scheme(0)
{
    // Initialize with empty spectrum data
    m_spectrum_data.resize(64, 0.0f); // Default to 64 bands
}

void SpectrumAnalyzerWidget::updateSpectrum(const float* spectrum_data, int num_bands)
{
    if (spectrum_data && num_bands > 0) {
        m_spectrum_data.resize(num_bands);
        float max_val = 0.0f;
        bool data_changed = false;
        
        for (int i = 0; i < num_bands; ++i) {
            float new_val = std::max(0.0f, std::min(1.0f, spectrum_data[i]));
            if (std::abs(new_val - m_spectrum_data[i]) > 0.001f) {
                data_changed = true;
            }
            m_spectrum_data[i] = new_val;
            max_val = std::max(max_val, m_spectrum_data[i]);
        }
        
        // Only redraw if data actually changed
        if (data_changed) {
            std::cout << "SpectrumAnalyzerWidget: Data changed, max_val=" << max_val << ", bands=" << num_bands << std::endl;
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

void SpectrumAnalyzerWidget::draw(Surface& surface)
{
    // Clear background
    uint32_t bg_color = surface.MapRGB(0, 0, 0); // Black background
    surface.FillRect(bg_color);
    
    // Draw based on visualization mode
    switch (m_visualization_mode) {
        case 0:
            drawBars(surface);
            break;
        case 1:
            drawOscilloscope(surface);
            break;
        default:
            drawBars(surface);
            break;
    }
}

void SpectrumAnalyzerWidget::drawBars(Surface& surface)
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    if (m_spectrum_data.empty() || width <= 0 || height <= 0) {
        std::cout << "SpectrumAnalyzerWidget::drawBars: No data or invalid size" << std::endl;
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
    
    std::cout << "SpectrumAnalyzerWidget::drawBars: " << num_bands << " bands, max_val=" << max_val << std::endl;
    
    for (int i = 0; i < num_bands; ++i) {
        float value = m_spectrum_data[i];
        int bar_height = std::max(1, static_cast<int>(value * height)); // Always at least 1 pixel
        
        int x = spacing + i * (bar_width + spacing);
        int y = height - bar_height;
        
        uint32_t color = getSpectrumColor(std::max(0.01f, value), i, surface); // Always get a color
        surface.box(x, y, x + bar_width - 1, height - 1, 
                   (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
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
        
        uint32_t color = getSpectrumColor(std::abs(value1), x, surface);
        surface.line(x1, y1, x2, y2, 
                    (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, 255);
    }
}

uint32_t SpectrumAnalyzerWidget::getSpectrumColor(float value, int position, Surface& surface)
{
    switch (m_color_scheme) {
        case 0: // Classic green
            {
                uint8_t intensity = static_cast<uint8_t>(value * 255);
                return surface.MapRGB(0, intensity, 0);
            }
        case 1: // Rainbow gradient
            {
                // Create rainbow based on position and intensity
                float hue = static_cast<float>(position) / m_spectrum_data.size() * 6.0f;
                int h = static_cast<int>(hue) % 6;
                float f = hue - h;
                float intensity = value;
                
                uint8_t v = static_cast<uint8_t>(intensity * 255);
                uint8_t p = 0;
                uint8_t q = static_cast<uint8_t>((1.0f - f) * intensity * 255);
                uint8_t t = static_cast<uint8_t>(f * intensity * 255);
                
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
        case 2: // Fire gradient (red/yellow)
            {
                uint8_t intensity = static_cast<uint8_t>(value * 255);
                if (value < 0.5f) {
                    return surface.MapRGB(intensity * 2, 0, 0);
                } else {
                    return surface.MapRGB(255, (intensity - 128) * 2, 0);
                }
            }
        default:
            return surface.MapRGB(static_cast<uint8_t>(value * 255), 
                                static_cast<uint8_t>(value * 255), 
                                static_cast<uint8_t>(value * 255));
    }
}