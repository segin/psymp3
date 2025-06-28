/*
 * FadingWidget.cpp - Implementation for a widget that can fade in and out.
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
 * WARRANTIES WITH REGARD to THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "psymp3.h"

FadingWidget::FadingWidget() : Widget()
{
    // The widget starts in a hidden state by default.
}

void FadingWidget::fadeIn()
{
    if (m_state == FadeState::Hidden || m_state == FadeState::FadingOut) {
        m_state = FadeState::FadingIn;
        m_state_change_time = SDL_GetTicks();
    }
}

void FadingWidget::fadeOut()
{
    if (m_state == FadeState::Visible || m_state == FadeState::FadingIn) {
        m_state = FadeState::FadingOut;
        m_state_change_time = SDL_GetTicks();
    }
}

bool FadingWidget::isHidden() const
{
    return m_state == FadeState::Hidden;
}

void FadingWidget::BlitTo(Surface& target)
{
    if (m_state == FadeState::Hidden) {
        return; // Don't draw if hidden.
    }

    Uint32 now = SDL_GetTicks();
    const float MAX_ALPHA = 255.0f;
    float alpha_f = MAX_ALPHA;

    // Update state machine
    if (m_state == FadeState::FadingIn && now >= m_state_change_time + m_fade_duration) {
        m_state = FadeState::Visible;
    }
    if (m_state == FadeState::FadingOut && now >= m_state_change_time + m_fade_duration) {
        m_state = FadeState::Hidden;
        return; // Became hidden this frame, so don't draw.
    }

    // Calculate alpha based on state
    if (m_state == FadeState::FadingIn) {
        float progress = std::clamp(static_cast<float>(now - m_state_change_time) / m_fade_duration, 0.0f, 1.0f);
        alpha_f = progress * MAX_ALPHA;
    } else if (m_state == FadeState::FadingOut) {
        float progress = std::clamp(static_cast<float>(now - m_state_change_time) / m_fade_duration, 0.0f, 1.0f);
        alpha_f = (1.0f - progress) * MAX_ALPHA;
    } else { // Visible
        alpha_f = MAX_ALPHA;
    }

    // Apply calculated alpha and blit
    this->SetAlpha(SDL_SRCALPHA, static_cast<Uint8>(alpha_f));
    Widget::BlitTo(target); // Call base class blit
}