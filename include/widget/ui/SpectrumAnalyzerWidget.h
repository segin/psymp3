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

namespace PsyMP3 {
namespace Widget {
namespace UI {

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

    /**
     * @brief Draws the spectrum on the GPU, in the display's underlay, from now on.
     *
     * The widget's own surface then stays transparent and is copied over
     * its area of the frame each time, clearing it so the underlay shows.
     * @param display The display whose underlay to draw in (it must have a renderer).
     */
    void useUnderlay(PsyMP3::Core::Display* display);

    /**
     * @brief Asks the display to show the underlay this frame, then blits as usual.
     * @param target The target surface to blit to
     * @param parent_absolute_pos The absolute position of the parent widget
     */
    void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos) override;

protected:
    /**
     * @brief Draws the spectrum visualization.
     * @param surface Surface to draw the spectrum on
     */
    virtual void draw(Surface& surface) override;

    /**
     * @brief Makes the kept spectrum surface: opaque XRGB8888, starting black.
     * @param width Surface width
     * @param height Surface height
     * @return The new surface.
     */
    std::unique_ptr<Surface> createSurface(int width, int height) override;

private:
    std::unique_ptr<Surface> m_fade_surface; // black, alpha-modulated over the trail each frame
    uint8_t m_fade_alpha_set = 255;          // the alpha last given to m_fade_surface
    PsyMP3::Core::Display* m_display = nullptr; // draw in its underlay, when set
    std::vector<SDL_FColor> m_gpu_colors;    // m_precomputed_colors as renderer colours
    std::vector<SDL_Vertex> m_vertices;      // the bars' triangles, kept to reuse the memory
    std::vector<int> m_indices;

    /**
     * @brief Fades the trail and draws this frame into the display's underlay.
     * @param surface The widget's own (transparent) surface, for the colours' pixel format.
     */
    void drawOnUnderlay(Surface& surface);

    /**
     * @brief The fade alpha for the current decay factor, as draw() has always computed it.
     * @return 0 (no fade) to 255 (clear to black).
     */
    uint8_t fadeAlpha() const;
    std::vector<float> m_spectrum_data;
    int m_visualization_mode;
    int m_color_scheme;
    float m_decay_factor;
    int m_scale_factor;
    // Start of the wall-clock redraw tail owed to let the fade/decay animation
    // finish after the input stops changing (e.g. a silent passage). Without
    // this the "redraw only on change" optimization freezes the fade mid-decay.
    // 0 = no activity seen yet.
    Uint32 m_fade_settle_start_ms = 0;

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

    /**
     * @brief Precomputes the color table for the current visualization settings.
     * @param surface Surface to use for color mapping
     */
    void precomputeColors(Surface& surface);

    std::vector<uint32_t> m_precomputed_colors;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // SPECTRUMANALYZERWIDGET_H