/*
 * ToastNotification.h - A temporary, on-screen notification widget.
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

#ifndef TOASTNOTIFICATION_H
#define TOASTNOTIFICATION_H

#include "font.h"

#include "font.h"

class ToastNotification : public Widget
{
public:
    ToastNotification(Font* font, const std::string& message, Uint32 visible_duration_ms, Uint32 fade_duration_ms = 350);
    bool isExpired() const;
    void BlitTo(Surface& target);
    void startFadeOut(); // Force immediate fade-out for smooth replacement

private:
    enum class State {
        FadingIn,
        Visible,
        FadingOut,
        Expired
    };

    State m_state;
    Uint32 m_creation_time;
    Uint32 m_expiration_time;
    Uint32 m_fade_duration;
    Uint32 m_visible_duration;
    Font* m_font; // Non-owning pointer to the global font

};

#endif // TOASTNOTIFICATION_H