/*
 * EqualizerWindow.h - Equalizer client widget (sliders, curve, menu, presets).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef EQUALIZERWINDOW_H
#define EQUALIZERWINDOW_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

using PsyMP3::Widget::Foundation::LayoutWidget;

// The content of the equalizer window: a menu bar (File: Load/Save; Presets),
// one vertical fader per band with a live dB readout and a frequency label, a
// smoothed response-curve preview, and an on/off checkbox.
//
// The widget owns the UI, the built-in presets, and the .psymp3eq file format.
// It delegates path selection and DSP application to the owner via callbacks:
//   - onBandChanged(band, dB) / onEnabledChanged(on): apply to the audio engine.
//   - onLoadRequested / onSaveRequested: pick a path (File dialog / prompt) and
//     call loadFromFile()/saveToFile().
class EqualizerWindow : public LayoutWidget {
public:
    EqualizerWindow(Font* font,
                    const std::vector<std::string>& band_labels,
                    double min_db, double max_db,
                    const std::vector<double>& initial_gains,
                    bool enabled);

    void setOnBandChanged(std::function<void(int, double)> cb) { m_on_band = std::move(cb); }
    void setOnEnabledChanged(std::function<void(bool)> cb)     { m_on_enabled = std::move(cb); }
    void setOnLoadRequested(std::function<void()> cb)          { m_on_load = std::move(cb); }
    void setOnSaveRequested(std::function<void()> cb)          { m_on_save = std::move(cb); }

    // Snap every slider to `gains` (propagates through the normal change path,
    // so the curve, readouts and onBandChanged all update).
    void applyGains(const std::vector<double>& gains);
    std::vector<double> currentGains() const { return m_gains; }

    bool loadFromFile(const std::string& path);        // parse + applyGains
    bool saveToFile(const std::string& path) const;    // write .psymp3eq

    static int preferredWidth(int num_bands);
    static int preferredHeight();

private:
    Font* m_font;
    int    m_num;
    double m_min_db;
    double m_max_db;
    std::vector<std::string> m_labels;
    std::vector<double>      m_gains;

    std::vector<SliderWidget*> m_sliders;
    std::vector<Label*>        m_value_labels;
    EqualizerCurveWidget*      m_curve = nullptr;
    MenuBarWidget*             m_menu = nullptr;
    CheckboxWidget*            m_enable = nullptr;

    std::function<void(int, double)> m_on_band;
    std::function<void(bool)>        m_on_enabled;
    std::function<void()>            m_on_load;
    std::function<void()>            m_on_save;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // EQUALIZERWINDOW_H
