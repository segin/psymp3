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

ToastNotification::ToastNotification(Font* font, const std::string& message, Uint32 duration_ms)
    : Widget() // Call base class constructor
{
    const int PADDING = 8;

    SDL_Color text_color = {230, 230, 230, 255}; // Light grey text
    // Create the label as a child of this widget
    auto label = std::make_unique<Label>(font, Rect(PADDING, PADDING, 0, 0), TagLib::String(message, TagLib::String::UTF8), text_color);

    // Set the size of this ToastNotification widget to fit the label plus padding
    m_pos.width(label->getPos().width() + (PADDING * 2));
    m_pos.height(label->getPos().height() + (PADDING * 2));

    // The background surface for the rounded box
    m_handle.reset(SDL_CreateRGBSurface(SDL_SWSURFACE | SDL_SRCALPHA, m_pos.width(), m_pos.height(), 32, 0, 0, 0, 0));
    if (!m_handle) {
        throw SDLException("Could not create surface for ToastNotification");
    }
    this->SetAlpha(SDL_SRCALPHA, 255); // Enable alpha blending for the surface
    this->roundedBoxRGBA(0, 0, m_pos.width() - 1, m_pos.height() - 1, 8, 50, 50, 50, 210);

    // Add the label as a child so it gets drawn relative to this widget
    addChild(std::move(label));

    m_expiration_time = SDL_GetTicks() + duration_ms;
}

bool ToastNotification::isExpired() const
{
    return SDL_GetTicks() >= m_expiration_time;
}