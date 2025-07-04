/*
 * MainUIWidget.h - Main background UI widget for the application
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

#ifndef MAINUIWIDGET_H
#define MAINUIWIDGET_H

#include "Widget.h"
#include "SpectrumAnalyzerWidget.h"
#include "PlayerProgressBarWidget.h"
#include <memory>

/**
 * @brief Main background UI widget that fills the screen behind all windows.
 * 
 * This widget acts as the "desktop" or background UI for the application.
 * It contains the spectrum analyzer, player controls, progress bar, and
 * other main UI elements. It always renders at the bottom of the z-order,
 * with all window widgets appearing on top of it.
 */
class MainUIWidget : public Widget {
public:
    /**
     * @brief Constructor for MainUIWidget.
     * @param width Full screen width
     * @param height Full screen height
     */
    MainUIWidget(int width, int height);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~MainUIWidget() = default;
    
    /**
     * @brief Updates the spectrum analyzer with new audio data.
     * @param spectrum_data Array of spectrum values (0.0 to 1.0)
     * @param num_bands Number of frequency bands in the spectrum data
     */
    void updateSpectrum(const float* spectrum_data, int num_bands);
    
    /**
     * @brief Updates the progress bar with current playback position.
     * @param position Current position (0.0 to 1.0)
     */
    void updateProgress(float position);
    
    /**
     * @brief Gets the spectrum analyzer widget for direct access.
     * @return Pointer to spectrum analyzer widget
     */
    SpectrumAnalyzerWidget* getSpectrumAnalyzer() const { return m_spectrum_analyzer; }
    
    /**
     * @brief Gets the progress bar widget for direct access.
     * @return Pointer to progress bar widget
     */
    PlayerProgressBarWidget* getProgressBar() const { return m_progress_bar; }

private:
    SpectrumAnalyzerWidget* m_spectrum_analyzer;
    PlayerProgressBarWidget* m_progress_bar;
    
    /**
     * @brief Rebuilds the main UI surface by compositing all elements.
     */
    void rebuildSurface();
    
    /**
     * @brief Sets up the layout of child widgets.
     */
    void layoutChildren();
};

#endif // MAINUIWIDGET_H