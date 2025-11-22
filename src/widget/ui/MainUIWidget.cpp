/*
 * MainUIWidget.cpp - Implementation for main background UI widget
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

MainUIWidget::MainUIWidget(int width, int height)
    : Widget()
{
    // Set to fill the entire screen
    Rect screen_rect(0, 0, width, height);
    setPos(screen_rect);
    
    // Create spectrum analyzer (takes up most of the screen)
    int spectrum_height = height - 60; // Leave space for progress bar and controls
    auto spectrum_analyzer = std::make_unique<SpectrumAnalyzerWidget>(width, spectrum_height);
    spectrum_analyzer->setPos(Rect(0, 0, width, spectrum_height));
    m_spectrum_analyzer = spectrum_analyzer.get(); // Keep pointer for direct access
    addChild(std::move(spectrum_analyzer));
    
    // Create progress bar at the bottom
    int progress_height = 40;
    int progress_y = height - progress_height - 10; // 10px margin from bottom
    auto progress_bar = std::make_unique<PlayerProgressBarWidget>(width - 20, progress_height); // 10px margin on each side
    progress_bar->setPos(Rect(10, progress_y, width - 20, progress_height));
    m_progress_bar = progress_bar.get(); // Keep pointer for direct access
    addChild(std::move(progress_bar));
    
    // Create the background surface
    rebuildSurface();
}

void MainUIWidget::updateProgress(float position)
{
    if (m_progress_bar) {
        m_progress_bar->setProgress(position);
    }
}

void MainUIWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    
    // Fill with dark background color
    uint32_t bg_color = surface->MapRGB(32, 32, 32); // Dark gray background
    surface->FillRect(bg_color);
    
    // The child widgets (spectrum analyzer and progress bar) will render themselves
    // when BlitTo is called by the ApplicationWidget
    
    setSurface(std::move(surface));
}

void MainUIWidget::layoutChildren()
{
    Rect pos = getPos();
    int width = pos.width();
    int height = pos.height();
    
    if (m_spectrum_analyzer) {
        int spectrum_height = height - 60;
        m_spectrum_analyzer->setPos(Rect(0, 0, width, spectrum_height));
    }
    
    if (m_progress_bar) {
        int progress_height = 40;
        int progress_y = height - progress_height - 10;
        m_progress_bar->setPos(Rect(10, progress_y, width - 20, progress_height));
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3