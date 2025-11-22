/*
 * ProgressBarFrameWidget.cpp - Container widget for progress bar frame and fill
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

namespace PsyMP3 {
namespace Widget {
namespace UI {

using Foundation::Widget;
using Foundation::DrawableWidget;

ProgressBarFrameWidget::ProgressBarFrameWidget()
    : LayoutWidget(222, 16, true) // Use LayoutWidget as base with transparent background
    , m_progress_bar(nullptr)
{
    // Child 1: Progress bar widget (drawn first, as background)
    auto progress_widget = std::make_unique<PlayerProgressBarWidget>(220, 10);
    m_progress_bar = addChildAt(std::move(progress_widget), 1, 3, 220, 10);
    
    // Child 2: Left bracket (drawn second, over progress bar)
    auto left_bracket = std::make_unique<ProgressBarLeftBracketWidget>();
    addChildAt(std::move(left_bracket), 0, 0, 4, 16);
    
    // Child 3: Right bracket (drawn third, over progress bar)
    auto right_bracket = std::make_unique<ProgressBarRightBracketWidget>();
    addChildAt(std::move(right_bracket), 218, 0, 4, 16);
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

