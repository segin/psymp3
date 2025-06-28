/*
 * FadingWidget.h - A widget that can fade in and out.
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

#ifndef FADINGWIDGET_H
#define FADINGWIDGET_H

#include "Widget.h"

class FadingWidget : public Widget {
public:
    FadingWidget();
    ~FadingWidget() override = default;

    void BlitTo(Surface& target) override;
    void fadeIn();
    void fadeOut();
    [[nodiscard]] bool isHidden() const;

private:
    enum class FadeState {
        FadingIn,
        Visible,
        FadingOut,
        Hidden
    };

    FadeState m_state = FadeState::Hidden;
    Uint32 m_fade_duration = 250;
    Uint32 m_state_change_time = 0;
};

#endif //FADINGWIDGET_H