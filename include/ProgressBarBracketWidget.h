/*
 * ProgressBarBracketWidget.h - Frame bracket widgets for progress bar
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

#ifndef PROGRESSBARBRACKETWIDGET_H
#define PROGRESSBARBRACKETWIDGET_H

#include "DrawableWidget.h"

/**
 * @brief Left bracket frame for progress bar.
 */
class ProgressBarLeftBracketWidget : public DrawableWidget {
public:
    ProgressBarLeftBracketWidget() : DrawableWidget(4, 16) {
        setMouseTransparent(true); // Allow mouse events to pass through to progress bar
    }
    
protected:
    virtual void draw(Surface& surface) override;
};

/**
 * @brief Right bracket frame for progress bar.
 */
class ProgressBarRightBracketWidget : public DrawableWidget {
public:
    ProgressBarRightBracketWidget() : DrawableWidget(4, 16) {
        setMouseTransparent(true); // Allow mouse events to pass through to progress bar
    }
    
protected:
    virtual void draw(Surface& surface) override;
};

#endif // PROGRESSBARBRACKETWIDGET_H