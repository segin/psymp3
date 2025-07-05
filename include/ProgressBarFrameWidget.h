/*
 * ProgressBarFrameWidget.h - Container widget for progress bar frame and fill
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

#ifndef PROGRESSBARFRAMEWIDGET_H
#define PROGRESSBARFRAMEWIDGET_H

#include "LayoutWidget.h"
#include "DrawableWidget.h"
#include "PlayerProgressBarWidget.h"
#include "ProgressBarBracketWidget.h"

/**
 * @brief Container widget that demonstrates widget hierarchy by holding frame and progress bar as children.
 * 
 * This widget creates the authoritative progress bar frame structure and contains
 * the actual progress bar widget as a child, demonstrating proper widget hierarchy.
 * Uses LayoutWidget for easy composition without requiring custom subclassing.
 */
class ProgressBarFrameWidget : public LayoutWidget {
public:
    /**
     * @brief Constructor for ProgressBarFrameWidget.
     * Creates the frame structure and progress bar as child widgets.
     */
    ProgressBarFrameWidget();
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~ProgressBarFrameWidget() = default;
    
    /**
     * @brief Gets a pointer to the contained progress bar widget.
     * @return Non-owning pointer to the progress bar widget
     */
    PlayerProgressBarWidget* getProgressBar() const { return m_progress_bar; }

private:
    PlayerProgressBarWidget* m_progress_bar; // Non-owning pointer to child widget
};


#endif // PROGRESSBARFRAMEWIDGET_H