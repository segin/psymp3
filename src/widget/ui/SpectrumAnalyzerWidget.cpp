/*
 * SpectrumAnalyzerWidget.cpp - Implementation for spectrum analyzer widget
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
    // draw() fades and draws onto its own opaque, black-backed surface.
    setOpaque(true);
    setRetainSurface(true);
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
        
        // Keep redrawing until the fade animation has fully settled, not only
        // when the data changes. The fade/decay is advanced inside draw(), so if
        // we stopped invalidating the moment the input went quiet (e.g. a silent
        // passage, where every frame is the same ~0), draw() would stop being
        // called and the fade would freeze mid-decay ("stuck" graph). Any change
        // or remaining energy re-arms a bounded tail of redraws long enough for
        // the trail to darken to black; after that we go idle again. The tail is
        // wall-clock (2s, ~60 frames at the 30 FPS default) rather than a frame
        // count so it doesn't shrink to nothing at high or unlimited FPS, where
        // 60 frames pass in well under the time the trail needs to reach black.
        static constexpr Uint32 FADE_SETTLE_MS = 2000;
        Uint32 now = SDL_GetTicks();
        if (data_changed || max_val > 0.001f) {
            m_fade_settle_start_ms = now;
        }
        if (m_fade_settle_start_ms != 0 && now - m_fade_settle_start_ms < FADE_SETTLE_MS) {
            invalidate(); // Trigger redraw (advances the fade in draw())
        }
    }
}

void SpectrumAnalyzerWidget::precomputeColors(Surface& surface)
{
    int num_bands = m_spectrum_data.size();
    if (num_bands <= 0) {
        m_precomputed_colors.clear();
        return;
    }

    m_precomputed_colors.clear(); // Ensure cache is not used during calculation
    std::vector<uint32_t> new_colors(num_bands);
    for (int i = 0; i < num_bands; ++i) {
        new_colors[i] = getSpectrumColor(0.0f, i, surface);
    }
    m_precomputed_colors = std::move(new_colors);
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
        m_precomputed_colors.clear(); // Force re-precomputation
        invalidate();
    }
}

void SpectrumAnalyzerWidget::setDecayFactor(float decay_factor)
{
    m_decay_factor = decay_factor;
}

/**
 * @brief Makes the spectrum's surface: opaque XRGB8888, starting black.
 *
 * The widget keeps this surface (setRetainSurface()), and draw() fades
 * and draws onto it in place, so the trail of earlier frames is the
 * surface's own pixels.
 * @param width Surface width.
 * @param height Surface height.
 * @return The new surface.
 */
std::unique_ptr<Surface> SpectrumAnalyzerWidget::createSurface(int width, int height)
{
    if (m_display) {
        /*
         * Drawing in the underlay: the surface is transparent and copied
         * rather than blended, so each frame it clears the spectrum's area
         * of the frame (as the opaque spectrum used to cover it) and lets
         * the underlay show through.
         */
        auto surface = std::make_unique<Surface>(width, height, true);
        surface->FillRect(surface->MapRGBA(0, 0, 0, 0));
        SDL_SetSurfaceBlendMode(surface->getHandle(), SDL_BLENDMODE_NONE);
        return surface;
    }
    auto surface = std::make_unique<Surface>(width, height);
    surface->FillRect(surface->MapRGB(0, 0, 0));
    return surface;
}

/**
 * @brief Switches to drawing in the display's underlay.
 * @param display The display to draw in.
 */
void SpectrumAnalyzerWidget::useUnderlay(PsyMP3::Core::Display* display)
{
    m_display = display;
    // The surface made so far (if any) was for the other way of drawing.
    setOpaque(display == nullptr);
    setSurface(nullptr);
    m_precomputed_colors.clear();
    invalidate();
}

/**
 * @brief Shows the underlay at this widget's place this frame, then blits as usual.
 * @param target The frame.
 * @param parent_absolute_pos The parent's absolute position.
 */
void SpectrumAnalyzerWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    if (m_display && isVisible()) {
        const Rect pos = getPos();
        m_display->showUnderlay(Rect(parent_absolute_pos.x() + pos.x(), parent_absolute_pos.y() + pos.y(),
                                     pos.width(), pos.height()));
    }
    DrawableWidget::recursiveBlitTo(target, parent_absolute_pos);
}

/**
 * @brief The fade alpha for the current decay factor.
 * @return 0 (no fade) to 255 (clear to black).
 */
uint8_t SpectrumAnalyzerWidget::fadeAlpha() const
{
    // Calculate alpha for fade (0-255). Original: decayfactor from 0.5 to 2.0
    // Original formula: (255 * (decayfactor / 4.0f))  where decayfactor=1.0 gives 63 alpha
    // Clamp before the cast: the decay factor arrives from --decay unchecked,
    // and a float-to-uint8_t conversion of an out-of-range value (factor > 4
    // or negative) is undefined behavior, not a saturating truncation.
    return static_cast<uint8_t>(std::clamp(255.0f * (m_decay_factor / 4.0f), 0.0f, 255.0f));
}

/**
 * @brief Fades the trail and draws the bars (or oscilloscope) in the underlay.
 * @param surface The widget's surface, whose format the colours were mapped to.
 */
void SpectrumAnalyzerWidget::drawOnUnderlay(Surface& surface)
{
    const Rect pos = getPos();
    const int width = pos.width();
    const int height = pos.height();
    bool fresh = false;
    SDL_Renderer* renderer = m_display->beginUnderlay(width, height, fresh);
    if (!renderer) {
        return;
    }

    if (m_gpu_colors.size() != m_precomputed_colors.size()) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surface.getHandle()->format);
        m_gpu_colors.resize(m_precomputed_colors.size());
        for (size_t i = 0; i < m_precomputed_colors.size(); ++i) {
            uint8_t r = 0, g = 0, b = 0;
            SDL_GetRGB(m_precomputed_colors[i], fmt, nullptr, &r, &g, &b);
            m_gpu_colors[i] = SDL_FColor{r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
        }
    }

    if (fresh) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    // The fade, as the CPU path's blit of black at fadeAlpha() (see Display::fadeBlendMode()).
    const uint8_t alpha = fadeAlpha();
    if (alpha > 0) {
        if (SDL_SetRenderDrawBlendMode(renderer, PsyMP3::Core::Display::fadeBlendMode())) {
            const float half = 0.5f / 255.0f;
            SDL_SetRenderDrawColorFloat(renderer, half, half, half, alpha / 255.0f);
        } else {
            // The software renderer's plain blend already rounds like the blit.
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
        }
        SDL_RenderFillRect(renderer, nullptr);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    const int num_bands = static_cast<int>(m_spectrum_data.size());
    const int draw_bands = std::min(num_bands, static_cast<int>(m_gpu_colors.size()));
    if (m_visualization_mode == 1 && draw_bands > 1) {
        // The oscilloscope, placed as drawOscilloscope() places it.
        const int num_points = std::min(draw_bands, width);
        const int center_y = height / 2;
        const int amplitude = height / 4;
        for (int x = 0; x < num_points - 1; ++x) {
            const float value1 = (m_spectrum_data[x] - 0.5f) * 2.0f;
            const float value2 = (m_spectrum_data[x + 1] - 0.5f) * 2.0f;
            const int y1 = std::clamp(center_y - static_cast<int>(value1 * amplitude), 0, height - 1);
            const int y2 = std::clamp(center_y - static_cast<int>(value2 * amplitude), 0, height - 1);
            const SDL_FColor& c = m_gpu_colors[x];
            SDL_SetRenderDrawColorFloat(renderer, c.r, c.g, c.b, 1.0f);
            SDL_RenderLine(renderer, static_cast<float>(x * width / num_points), static_cast<float>(y1),
                           static_cast<float>((x + 1) * width / num_points), static_cast<float>(y2));
        }
    } else if (draw_bands > 0) {
        // The bars, placed as drawBars() places them, in one call.
        const int bar_width = std::max(1, width / num_bands);
        const int spacing = std::max(0, (width - (bar_width * num_bands)) / (num_bands + 1));
        m_vertices.clear();
        m_indices.clear();
        for (int i = 0; i < draw_bands; ++i) {
            const int y_start = std::clamp(static_cast<int>(height - (m_spectrum_data[i] * height)), 0, height - 1);
            const float x0 = static_cast<float>(spacing + i * (bar_width + spacing));
            const float x1 = x0 + static_cast<float>(bar_width);
            const float y0 = static_cast<float>(y_start);
            const float y1 = static_cast<float>(height);
            const SDL_FColor& c = m_gpu_colors[i];
            const int base = static_cast<int>(m_vertices.size());
            m_vertices.push_back(SDL_Vertex{{x0, y0}, c, {0.0f, 0.0f}});
            m_vertices.push_back(SDL_Vertex{{x1, y0}, c, {0.0f, 0.0f}});
            m_vertices.push_back(SDL_Vertex{{x1, y1}, c, {0.0f, 0.0f}});
            m_vertices.push_back(SDL_Vertex{{x0, y1}, c, {0.0f, 0.0f}});
            for (int k : {0, 1, 2, 0, 2, 3}) {
                m_indices.push_back(base + k);
            }
        }
        SDL_RenderGeometry(renderer, nullptr, m_vertices.data(), static_cast<int>(m_vertices.size()),
                           m_indices.data(), static_cast<int>(m_indices.size()));
    }

    m_display->endUnderlay();
}

void SpectrumAnalyzerWidget::draw(Surface& spectrum_surface)
{
    const int width = spectrum_surface.width();
    const int height = spectrum_surface.height();

    if (m_precomputed_colors.size() != m_spectrum_data.size() || m_precomputed_colors.empty()) {
        precomputeColors(spectrum_surface);
        m_gpu_colors.clear();
    }

    if (m_display) {
        drawOnUnderlay(spectrum_surface);
        return;
    }

    // Create fade surface once and reuse (matches original algorithm)
    if (!m_fade_surface || m_fade_surface->width() != width || m_fade_surface->height() != height) {
        m_fade_surface = std::make_unique<Surface>(width, height);
        m_fade_surface->FillRect(m_fade_surface->MapRGB(0, 0, 0));
        m_fade_alpha_set = 255; // a new surface has no alpha modulation
    }
    Surface& fade_surface = *m_fade_surface;
    
    const uint8_t fade_alpha = fadeAlpha();

    // Only call SetAlpha if the fade_alpha has changed to avoid redundant SDL calls
    if (fade_alpha != m_fade_alpha_set) {
        fade_surface.SetAlpha(fade_alpha);
        m_fade_alpha_set = fade_alpha;
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
    
    
    // Defensive: never index m_precomputed_colors past its real length even if
    // the caller-enforced invariant (colors.size() == spectrum.size(), set up in
    // draw()) is ever violated. The bar layout above still uses the full
    // num_bands. NOTE: m_spectrum_data/m_precomputed_colors are unsynchronized;
    // updateSpectrum() and draw() must run on the same (UI) thread.
    int draw_bands = std::min(num_bands, static_cast<int>(m_precomputed_colors.size()));
    for (int i = 0; i < draw_bands; ++i) {
        float value = m_spectrum_data[i];
        // Calculate y position like the original: from amplitude down to bottom
        int y_start = static_cast<int>(height - (value * height));
        y_start = std::max(0, std::min(height - 1, y_start)); // Clamp to valid range

        int x = spacing + i * (bar_width + spacing);

        // Use precomputed color
        uint32_t color = m_precomputed_colors[i];
        
        // Draw rectangle from y_start down to bottom (like original rectangle call)
        surface.box(x, y_start, x + bar_width - 1, height - 1, color);
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
    
    // Check cache first (unless we are currently precomputing)
    if (!m_precomputed_colors.empty() && position >= 0 && position < static_cast<int>(m_precomputed_colors.size())) {
        return m_precomputed_colors[position];
    }

    switch (m_color_scheme) {
        case 0: // Original spectrum colors (exact old renderSpectrum algorithm)
            {
                // Use position directly (should be 0-319 for 320 bands)
                auto color = PsyMP3::Core::SpectrumConfig::getBarColor(static_cast<uint16_t>(position));
                return surface.MapRGB(color.r, color.g, color.b);
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
