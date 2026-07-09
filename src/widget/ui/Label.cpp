/*
 * Label.cpp - A text label widget implementation.
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

Label::Label(Font* font, const Rect& position, const TagLib::String& initial_text, SDL_Color color, SDL_Color background_color)
    : Widget(Surface(), position), // Initialize base Widget with an empty surface and a position
      m_font(font),
      m_text(), // Will be set by setText
      m_color(color),
      m_background_color(background_color)
{
    // The initial render is done by calling setText.
    setText(initial_text);
}

void Label::setBackgroundColor(SDL_Color background_color)
{
    if (background_color.r == m_background_color.r
        && background_color.g == m_background_color.g
        && background_color.b == m_background_color.b
        && background_color.a == m_background_color.a) {
        return;
    }
    m_background_color = background_color;

    // The LCD render path bakes the bg colour into each glyph's edge pixels,
    // so a bg change requires re-rendering the text — invalidate alone leaves
    // glyphs blended against the previous bg and shows colour fringing.
    if (!m_text.isEmpty() && m_font) {
        m_text_surface = m_font->RenderLCD(m_text,
                                           m_color.r, m_color.g, m_color.b,
                                           m_background_color.r,
                                           m_background_color.g,
                                           m_background_color.b);
        if (m_text_surface) {
            auto widget_surface = std::make_unique<Surface>(m_text_surface->width(),
                                                            m_text_surface->height(),
                                                            true);
            widget_surface->FillRect(widget_surface->MapRGBA(0, 0, 0, 0));
            widget_surface->Blit(*m_text_surface,
                                 Rect(0, 0, m_text_surface->width(), m_text_surface->height()));
            setSurface(std::move(widget_surface));
        }
    }
    invalidate();
}

void Label::setMarqueeEnabled(bool enabled)
{
    if (m_marquee_enabled == enabled) {
        return;
    }

    m_marquee_enabled = enabled;
    invalidate();
}

void Label::setText(const TagLib::String& text)
{
    // Avoid re-rendering if the text hasn't changed.
    if (text == m_text) {
        return;
    }

    m_text = text;

    // A Label may be constructed with a null font; guard the render paths that
    // dereference it rather than crash.
    if (!m_font) {
        return;
    }

    // Use the ClearType (LCD subpixel) path so the rendered glyphs are
    // pre-blended against this label's background colour, avoiding the
    // single-alpha quality loss of compositing the text surface afterwards.
    m_text_surface = m_font->RenderLCD(m_text,
                                       m_color.r, m_color.g, m_color.b,
                                       m_background_color.r,
                                       m_background_color.g,
                                       m_background_color.b);
    if (!m_text_surface) {
        std::cerr << "Failed to render text surface for label." << std::endl;
        return;
    }

    auto widget_surface = std::make_unique<Surface>(m_text_surface->width(), m_text_surface->height(), true);
    widget_surface->FillRect(widget_surface->MapRGBA(0, 0, 0, 0));
    widget_surface->Blit(*m_text_surface, Rect(0, 0, m_text_surface->width(), m_text_surface->height()));
    setSurface(std::move(widget_surface));
    
    // Notify parent that this widget needs repainting
    invalidate();
}

void Label::BlitTo(Surface& target)
{
    if (!m_visible) {
        return;
    }
    blitWithBackgroundClear(target, m_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, m_pos);
    }
}

void Label::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    if (!m_visible) {
        return;
    }
    Rect absolute_pos(parent_absolute_pos.x() + m_pos.x(),
                      parent_absolute_pos.y() + m_pos.y(),
                      m_pos.width(), m_pos.height());

    blitWithBackgroundClear(target, absolute_pos);

    // Blit children (if any)
    for (const auto& child : m_children) {
        child->recursiveBlitTo(target, absolute_pos);
    }
}

void Label::blitWithBackgroundClear(Surface& target, const Rect& absolute_pos)
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return;
    }

    const int viewport_width = absolute_pos.width() > 0 ? absolute_pos.width() : m_text_surface->width();
    const int viewport_height = absolute_pos.height() > 0 ? absolute_pos.height() : m_text_surface->height();
    const bool should_marquee = m_marquee_enabled &&
                                viewport_width > 0 &&
                                m_text_surface->width() > viewport_width;

    // Labels render onto a persistent graph surface, so the old glyph bounds
    // must be cleared before alpha-blending the newly rendered text.
    int clear_w = std::max(viewport_width, m_last_drawn_width);
    int clear_h = std::max(viewport_height, m_last_drawn_height);

    target.box(absolute_pos.x(), absolute_pos.y(),
               absolute_pos.x() + clear_w - 1,
               absolute_pos.y() + clear_h - 1,
               target.MapRGB(m_background_color.r, m_background_color.g, m_background_color.b));

    if (should_marquee) {
        auto viewport_surface = createViewportSurface(viewport_width, viewport_height);
        if (viewport_surface && viewport_surface->isValid()) {
            target.Blit(*viewport_surface,
                        Rect(absolute_pos.x(), absolute_pos.y(), viewport_width, viewport_height));
        }
    } else {
        target.Blit(*m_text_surface,
                    Rect(absolute_pos.x(), absolute_pos.y(),
                         m_text_surface->width(), m_text_surface->height()));
    }

    m_last_drawn_width = viewport_width;
    m_last_drawn_height = viewport_height;
}

std::unique_ptr<Surface> Label::createViewportSurface(int viewport_width, int viewport_height) const
{
    auto viewport_surface = std::make_unique<Surface>(viewport_width, viewport_height, true);
    viewport_surface->FillRect(viewport_surface->MapRGBA(0, 0, 0, 0));

    const uint32_t tick_ms = SDL_GetTicks();
    const int marquee_offset = calculateMarqueeOffset(tick_ms);
    const int text_y = std::max(0, (viewport_height - m_text_surface->height()) / 2);

    viewport_surface->Blit(*m_text_surface,
                           Rect(-marquee_offset, text_y,
                                m_text_surface->width(), m_text_surface->height()));
    viewport_surface->Blit(*m_text_surface,
                           Rect(m_text_surface->width() + kMarqueeGapPixels - marquee_offset,
                                text_y,
                                m_text_surface->width(), m_text_surface->height()));
    applyEdgeFade(*viewport_surface, calculateLeftEdgeFadeStrength(tick_ms));
    return viewport_surface;
}

int Label::calculateMarqueeOffset(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return 0;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return 0;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));
    if (phase < kMarqueePauseMs) {
        return 0;
    }

    const int scroll_phase_ms = phase - kMarqueePauseMs;
    return std::min((scroll_phase_ms * kMarqueePixelsPerSecond) / 1000, loop_width - 1);
}

bool Label::isInMarqueeHomePause(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return false;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return false;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));
    return phase < kMarqueePauseMs;
}

float Label::calculateLeftEdgeFadeStrength(uint32_t tick_ms) const
{
    if (!m_text_surface || !m_text_surface->isValid()) {
        return 0.0f;
    }

    const int loop_width = m_text_surface->width() + kMarqueeGapPixels;
    if (loop_width <= 0) {
        return 0.0f;
    }

    const int scroll_duration_ms = std::max((loop_width * 1000) / kMarqueePixelsPerSecond, 1);
    const int cycle_ms = kMarqueePauseMs + scroll_duration_ms;
    const int phase = static_cast<int>(tick_ms % static_cast<uint32_t>(cycle_ms));

    if (phase < kMarqueePauseMs) {
        if (phase >= kMarqueePauseMs - kGradientTransitionMs) {
            const int transition_phase = phase - (kMarqueePauseMs - kGradientTransitionMs);
            return std::clamp(static_cast<float>(transition_phase) /
                                  static_cast<float>(kGradientTransitionMs),
                              0.0f, 1.0f);
        }
        return 0.0f;
    }

    const int scroll_phase = phase - kMarqueePauseMs;
    if (scroll_phase >= scroll_duration_ms - kGradientTransitionMs) {
        const int transition_phase = scroll_phase - (scroll_duration_ms - kGradientTransitionMs);
        return std::clamp(1.0f - (static_cast<float>(transition_phase) /
                                      static_cast<float>(kGradientTransitionMs)),
                          0.0f, 1.0f);
    }

    return 1.0f;
}

void Label::applyEdgeFade(Surface& surface, float left_fade_strength) const
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return;
    }

    const int fade_width = std::min(kEdgeFadeWidth, surface.width() / 2);
    if (fade_width <= 0) {
        return;
    }

    const bool must_lock = SDL_MUSTLOCK(handle);
    if (must_lock && SDL_LockSurface(handle) != 0) {
        return;
    }

    for (int y = 0; y < surface.height(); ++y) {
        auto* row = static_cast<uint8_t*>(handle->pixels) + y * handle->pitch;
        for (int x = 0; x < surface.width(); ++x) {
            float fade = 1.0f;
            if (left_fade_strength > 0.0f && x < fade_width) {
                const float edge_fade = static_cast<float>(x) / static_cast<float>(fade_width);
                fade = ((1.0f - left_fade_strength) * 1.0f) + (left_fade_strength * edge_fade);
            } else if (x >= surface.width() - fade_width) {
                fade = static_cast<float>((surface.width() - 1) - x) / static_cast<float>(fade_width);
            }

            if (fade >= 1.0f) {
                continue;
            }

            fade = std::clamp(fade, 0.0f, 1.0f);
            uint32_t pixel = 0;
            std::memcpy(&pixel, row + x * handle->format->BytesPerPixel, handle->format->BytesPerPixel);

            uint8_t r = 0;
            uint8_t g = 0;
            uint8_t b = 0;
            uint8_t a = 0;
            SDL_GetRGBA(pixel, handle->format, &r, &g, &b, &a);
            if (a == 0) {
                continue;
            }

            a = static_cast<uint8_t>(static_cast<float>(a) * fade);
            pixel = SDL_MapRGBA(handle->format, r, g, b, a);
            std::memcpy(row + x * handle->format->BytesPerPixel, &pixel, handle->format->BytesPerPixel);
        }
    }

    if (must_lock) {
        SDL_UnlockSurface(handle);
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
