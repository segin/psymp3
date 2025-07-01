/*
 * ToastNotification.cpp - Implementation for the on-screen notification widget.
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

ToastNotification::ToastNotification(Font* font, const std::string& message, Uint32 visible_duration_ms, Uint32 fade_duration_ms)
    : Widget() // Call base class constructor
{
    m_state = State::FadingIn;
    m_creation_time = SDL_GetTicks();
    m_fade_duration = fade_duration_ms;
    m_visible_duration = visible_duration_ms;
    // The expiration time now marks the *start* of the fade-out period.
    m_expiration_time = m_creation_time + m_fade_duration + m_visible_duration;

    // Refactored to use proper alpha blending instead of color keying.
    const int PADDING = 8;
    SDL_Color text_color = {255, 255, 255, 255}; // Opaque white for text

    // 1. Render text to a temporary surface to get its dimensions.
    auto text_sfc = font->Render(TagLib::String(message, TagLib::String::UTF8), text_color.r, text_color.g, text_color.b);
    if (!text_sfc || !text_sfc->isValid()) {
        throw std::runtime_error("Failed to render text for ToastNotification");
    }
    // Ensure the text surface itself is set up for alpha blending (for anti-aliasing).
    text_sfc->SetAlpha(SDL_SRCALPHA, 255);

    // 2. Create the main widget surface with full alpha support.
    int toast_width = text_sfc->width() + (PADDING * 2);
    int toast_height = text_sfc->height() + (PADDING * 2);
    auto toast_surface = std::make_unique<Surface>(toast_width, toast_height);
    if (!toast_surface) {
        throw SDLException("Could not create surface for ToastNotification");
    }

    // 3. Draw the background directly onto the new alpha-enabled surface.
    toast_surface->roundedBoxRGBA(0, 0, toast_width - 1, toast_height - 1, 8, 100, 100, 100, 255);
    toast_surface->roundedBoxRGBA(1, 1, toast_width - 2, toast_height - 2, 7, 50, 50, 50, 255);

    // 4. Blit the text onto the background.
    Rect text_dest_rect(PADDING, PADDING, 0, 0);
    toast_surface->Blit(*text_sfc, text_dest_rect);

    // 5. Set the final composed surface as the widget's surface.
    setSurface(std::move(toast_surface));
}

bool ToastNotification::isExpired() const
{
    // The widget is fully expired only after it has finished fading out.
    return m_state == State::Expired;
}

void ToastNotification::BlitTo(Surface& target)
{
    Uint32 now = SDL_GetTicks();
    const float MAX_ALPHA = 180.0f; // Set base translucency here
    float alpha_f = 0.0f;

    // Update state machine
    if (m_state == State::FadingIn && now >= m_creation_time + m_fade_duration) m_state = State::Visible;
    if (m_state == State::Visible && now >= m_expiration_time) m_state = State::FadingOut;
    if (m_state == State::FadingOut && now >= m_expiration_time + m_fade_duration) m_state = State::Expired;

    // Calculate alpha based on state
    if (m_state == State::FadingIn) {
        if (m_fade_duration > 0) {
            // Clamp the progress to prevent over/undershooting due to timing fluctuations.
            float progress = std::clamp(static_cast<float>(now - m_creation_time) / m_fade_duration, 0.0f, 1.0f);
            alpha_f = progress * MAX_ALPHA;
        } else {
            alpha_f = MAX_ALPHA;
        }
    } else if (m_state == State::Visible) {
        alpha_f = MAX_ALPHA;
    } else if (m_state == State::FadingOut) {
        if (m_fade_duration > 0) {
            float progress = std::clamp(static_cast<float>(now - m_expiration_time) / m_fade_duration, 0.0f, 1.0f);
            alpha_f = (1.0f - progress) * MAX_ALPHA;
        } else {
            alpha_f = 0;
        }
    } else { // Expired
        return;
    }

    // Apply calculated alpha and blit.
    // On a surface *without* per-pixel alpha, this enables global alpha blending for the blit operation.
    this->SetAlpha(SDL_SRCALPHA, static_cast<Uint8>(alpha_f));
    Widget::BlitTo(target); // Call base class blit
}