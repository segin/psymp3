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
// The widget owns the UI, the built-in presets, the five on-disk user preset
// slots, and the .psymp3eq file format. It applies DSP changes via callbacks:
//   - onBandChanged(band, dB) / onEnabledChanged(on): apply to the audio engine.
//   - onStatus(msg): optional; surfaced by the owner (e.g. as a toast).
class EqualizerWindow : public LayoutWidget {
public:
    static constexpr int kUserPresets = 5;

    EqualizerWindow(Font* font,
                    const std::vector<std::string>& band_labels,
                    double min_db, double max_db,
                    const std::vector<double>& initial_gains,
                    bool enabled,
                    double initial_volume);

    void setOnBandChanged(std::function<void(int, double)> cb) { m_on_band = std::move(cb); }
    void setOnEnabledChanged(std::function<void(bool)> cb)     { m_on_enabled = std::move(cb); }
    void setOnVolumeChanged(std::function<void(double)> cb)    { m_on_volume = std::move(cb); }
    void setOnStatus(std::function<void(const std::string&)> cb) { m_on_status = std::move(cb); }

    // Reflect an externally-driven volume change (keyboard Up/Down) on the
    // slider/readout without echoing it back through onVolumeChanged.
    void setVolume(double volume01);

    // Snap every slider to `gains` (propagates through the normal change path,
    // so the curve, readouts and onBandChanged all update).
    void applyGains(const std::vector<double>& gains);
    std::vector<double> currentGains() const { return m_gains; }

    bool loadFromFile(const std::string& path);        // parse + applyGains
    bool saveToFile(const std::string& path) const;    // write .psymp3eq

    // Offer a key to this window's menu bar (Alt+mnemonic to open; arrows/Enter/
    // Esc/mnemonics to navigate while open). Returns true if the menu consumed it.
    bool handleMenuKey(const SDL_keysym& keysym);

    // User preset slots (1..kUserPresets), stored as .psymp3eq files in the
    // config directory.
    void loadUserPreset(int slot);
    void saveUserPreset(int slot);
    static std::string userPresetPath(int slot);

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
    SliderWidget*              m_volume_slider = nullptr;
    Label*                     m_volume_value = nullptr;
    bool                       m_suppress_volume_cb = false;

    std::function<void(int, double)>        m_on_band;
    std::function<void(bool)>               m_on_enabled;
    std::function<void(double)>             m_on_volume;
    std::function<void(const std::string&)> m_on_status;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // EQUALIZERWINDOW_H
