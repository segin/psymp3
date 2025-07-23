/*
 * SpectrumAnalyzerWidget.h - Spectrum analyzer visualization widget
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

#ifndef SPECTRUMANALYZERWIDGET_H
#define SPECTRUMANALYZERWIDGET_H

// No direct includes - all includes should be in psymp3.h

/**
 * @brief Widget that renders audio spectrum analysis visualization.
 * 
 * This widget displays real-time audio spectrum data using bar graphs,
 * oscilloscope, or other visualization modes. It extends DrawableWidget
 * to provide custom rendering of the spectrum data.
 */
class SpectrumAnalyzerWidget : public DrawableWidget {
public:
    /**
     * @brief Constructor for SpectrumAnalyzerWidget.
     * @param width Widget width
     * @param height Widget height
     */
    SpectrumAnalyzerWidget(int width, int height);
    
    /**
     * @brief Virtual destructor.
     */
    virtual ~SpectrumAnalyzerWidget() = default;
    
    /**
     * @brief Updates the spectrum data and triggers a redraw.
     * @param spectrum_data Array of spectrum values (0.0 to 1.0)
     * @param num_bands Number of frequency bands in the spectrum data
     * @param scale_factor Scale factor for logarithmic scaling
     * @param decay_factor Decay factor for fade effect
     */
    void updateSpectrum(const float* spectrum_data, int num_bands, int scale_factor, float decay_factor);
    
    /**
     * @brief Sets the visualization mode.
     * @param mode Visualization mode (0=bars, 1=oscilloscope, etc.)
     */
    void setVisualizationMode(int mode);
    
    /**
     * @brief Sets the color scheme for the visualization.
     * @param color_scheme Color scheme index
     */
    void setColorScheme(int color_scheme);
    
    /**
     * @brief Sets the decay factor for the fade effect.
     * @param decay_factor Decay factor (default 1.0, where 1.0 = 63 alpha)
     */
    void setDecayFactor(float decay_factor);

protected:
    /**
     * @brief Draws the spectrum visualization.
     * @param surface Surface to draw the spectrum on
     */
    virtual void draw(Surface& surface) override;

private:
    std::vector<float> m_spectrum_data;
    int m_visualization_mode;
    int m_color_scheme;
    float m_decay_factor;
    int m_scale_factor;
    
    /**
     * @brief Draws spectrum bars visualization.
     * @param surface Surface to draw on
     */
    void drawBars(Surface& surface);
    
    /**
     * @brief Draws oscilloscope visualization.
     * @param surface Surface to draw on
     */
    void drawOscilloscope(Surface& surface);
    
    /**
     * @brief Gets a color for the given spectrum value and position.
     * @param value Spectrum value (0.0 to 1.0)
     * @param position Position index (for gradients)
     * @return RGB color value
     */
    uint32_t getSpectrumColor(float value, int position, Surface& surface);
};

#endif // SPECTRUMANALYZERWIDGET_H