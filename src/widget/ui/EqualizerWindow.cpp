/*
 * EqualizerWindow.cpp - Equalizer client widget implementation.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"

namespace PsyMP3 {
namespace Widget {
namespace UI {

namespace {

// --- Layout geometry (logical px) ---
constexpr int kMenuH   = MenuBarWidget::BAR_H;
constexpr int kPad     = 10;
constexpr int kCol     = 34;   // per-band column width
constexpr int kSliderW = 18;
constexpr int kSliderH = 110;
constexpr int kCurveH  = 64;
constexpr int kLabelH  = 12;

// --- Built-in presets (dB per band, 7-band layout) ---
// The table is written for exactly the DSP's band layout; if the band count
// ever changes, these curves must be redrawn, not silently truncated/padded.
static_assert(PsyMP3::DSP::Equalizer::kNumBands == 7,
              "built-in EQ presets are authored for 7 bands");
struct Preset { const char* name; double g[7]; };
constexpr Preset kPresets[] = {
    { "Flat",         {  0,  0,  0,  0,  0,  0,  0 } },
    { "Rock",         {  5,  3, -1, -2,  1,  3,  5 } },
    { "Pop",          { -1,  2,  4,  4,  2, -1, -2 } },
    { "Jazz",         {  3,  2,  0,  2, -1,  1,  3 } },
    { "Classical",    {  4,  2,  0,  0,  0,  2,  4 } },
    { "Dance",        {  6,  4,  1,  0, -2,  2,  4 } },
    { "Bass Boost",   {  7,  5,  3,  1,  0,  0,  0 } },
    { "Treble Boost", {  0,  0,  0,  1,  3,  5,  7 } },
    { "Vocal",        { -3, -1,  3,  5,  3,  1, -2 } },
};

std::string fmtDb(double db)
{
    long v = std::lround(db);
    if (v > 0) return "+" + std::to_string(v);
    return std::to_string(v);
}

std::string fmtPct(double volume01)
{
    return std::to_string(std::lround(volume01 * 100.0)) + "%";
}
} // namespace

int EqualizerWindow::preferredWidth(int num_bands)  { return 2 * kPad + num_bands * kCol; }
int EqualizerWindow::preferredHeight()
{
    // menu + curve + value labels + sliders + freq labels + enable + volume rows
    return kMenuH + 6 + kCurveH + 4 + kLabelH + 2 + kSliderH + 2 + kLabelH + 6 + 16 + 6 + 16 + kPad;
}

EqualizerWindow::EqualizerWindow(Font* font,
                                 const std::vector<std::string>& band_labels,
                                 double min_db, double max_db,
                                 const std::vector<double>& initial_gains,
                                 bool enabled,
                                 double initial_volume)
    : LayoutWidget(preferredWidth(static_cast<int>(band_labels.size())), preferredHeight(), false)
    , m_font(font)
    , m_num(static_cast<int>(band_labels.size()))
    , m_min_db(min_db)
    , m_max_db(max_db)
    , m_labels(band_labels)
    , m_gains(static_cast<size_t>(m_num), 0.0)
{
    for (int i = 0; i < m_num && i < static_cast<int>(initial_gains.size()); ++i)
        m_gains[i] = initial_gains[i];

    setBackgroundColor(192, 192, 192, 255); // Win9x face grey

    const int W = getPos().width();
    const int curve_y = kMenuH + 6;
    const int vlabel_y = curve_y + kCurveH + 4;
    const int slider_y = vlabel_y + kLabelH + 2;
    const int flabel_y = slider_y + kSliderH + 2;
    const int enable_y = flabel_y + kLabelH + 6;
    const int volume_y = enable_y + 16 + 6;

    // Curve preview.
    m_curve = addChildAt(
        std::make_unique<EqualizerCurveWidget>(W - 2 * kPad, kCurveH, m_num, m_min_db, m_max_db),
        kPad, curve_y, W - 2 * kPad, kCurveH);
    m_curve->setGains(m_gains);

    // One column per band: dB readout, fader, frequency label. Labels paint
    // black text on white (a Label's background is opaque, so an explicit
    // colour is required either way; white matches the checkbox).
    const SDL_Color label_fg{0, 0, 0, 255};
    const SDL_Color label_bg{255, 255, 255, 255};
    for (int i = 0; i < m_num; ++i) {
        const int col_x = kPad + i * kCol;
        const int sx = col_x + (kCol - kSliderW) / 2;

        Label* vlabel = addChildAt(
            std::make_unique<Label>(m_font, Rect(0, 0, kCol, kLabelH), fmtDb(m_gains[i]), label_fg, label_bg),
            col_x, vlabel_y, kCol, kLabelH);
        vlabel->setAlignment(Label::Align::Center);
        m_value_labels.push_back(vlabel);

        auto slider = std::make_unique<SliderWidget>(kSliderW, kSliderH, m_min_db, m_max_db,
                                                     m_gains[i], SliderOrientation::Vertical);
        SliderWidget* sp = slider.get();
        const int band = i;
        sp->setOnChange([this, band](double db) {
            m_gains[band] = db;
            if (m_curve) m_curve->setBandGain(band, db);
            if (band < static_cast<int>(m_value_labels.size()))
                m_value_labels[band]->setText(fmtDb(db));
            if (m_on_band) m_on_band(band, db);
        });
        addChildAt(std::move(slider), sx, slider_y, kSliderW, kSliderH);
        m_sliders.push_back(sp);

        Label* flabel = addChildAt(std::make_unique<Label>(m_font, Rect(0, 0, kCol, kLabelH),
                                           TagLib::String(m_labels[i], TagLib::String::UTF8), label_fg, label_bg),
                   col_x, flabel_y, kCol, kLabelH);
        flabel->setAlignment(Label::Align::Center);
    }

    // Enable checkbox.
    m_enable = addChildAt(std::make_unique<CheckboxWidget>(W - 2 * kPad, 16, m_font,
                                                           TagLib::String("Enable equalizer", TagLib::String::UTF8),
                                                           enabled),
                          kPad, enable_y, W - 2 * kPad, 16);
    m_enable->setOnToggle([this](bool on) { if (m_on_enabled) m_on_enabled(on); });

    // Volume row: label, horizontal slider (0..1), percentage readout.
    {
        const int name_w = 44;
        const int pct_w = 32;
        const int vs_x = kPad + name_w + 4;
        const int vs_w = W - kPad - pct_w - 4 - vs_x;

        addChildAt(std::make_unique<Label>(m_font, Rect(0, 0, name_w, kLabelH),
                                           TagLib::String("Volume", TagLib::String::UTF8),
                                           label_fg, label_bg),
                   kPad, volume_y + 2, name_w, kLabelH);

        auto vslider = std::make_unique<SliderWidget>(vs_w, 16, 0.0, 1.0, initial_volume,
                                                      SliderOrientation::Horizontal);
        m_volume_slider = vslider.get();
        m_volume_slider->setOnChange([this](double v) {
            if (m_volume_value) m_volume_value->setText(fmtPct(v));
            if (!m_suppress_volume_cb && m_on_volume) m_on_volume(v);
        });
        addChildAt(std::move(vslider), vs_x, volume_y, vs_w, 16);

        m_volume_value = addChildAt(std::make_unique<Label>(m_font, Rect(0, 0, pct_w, kLabelH),
                                                            fmtPct(initial_volume), label_fg, label_bg),
                                    W - kPad - pct_w, volume_y + 2, pct_w, kLabelH);
        m_volume_value->setAlignment(Label::Align::Right);
    }

    // Menu bar on top (full-surface pass-through overlay; added last => topmost).
    auto menu = std::make_unique<MenuBarWidget>(getPos().width(), getPos().height(), m_font);
    m_menu = menu.get();
    using MI = MenuBarWidget::Item;

    // Built-in presets.
    std::vector<MI> preset_items;
    for (const auto& p : kPresets) {
        std::vector<double> gains(p.g, p.g + 7);
        preset_items.push_back(MI::leaf(p.name, [this, gains] { applyGains(gains); }));
    }
    // Note: no '&' mnemonics here — the keyboard driver (handleKey) is only
    // wired to the player's main menu bar, so underlined accelerators in this
    // window would advertise keys that do nothing.
    m_menu->addMenu("Presets", std::move(preset_items));

    // User presets: slots 1..N load directly; the Save submenu stores into a slot.
    std::vector<MI> user_items;
    std::vector<MI> save_items;
    for (int slot = 1; slot <= kUserPresets; ++slot) {
        std::string name = "User preset " + std::to_string(slot);
        user_items.push_back(MI::leaf(name, [this, slot] { loadUserPreset(slot); }));
        save_items.push_back(MI::leaf(name, [this, slot] { saveUserPreset(slot); }));
    }
    user_items.push_back(MI::sep());
    user_items.push_back(MI::sub("Save", std::move(save_items)));
    m_menu->addMenu("User Presets", std::move(user_items));

    addChildAt(std::move(menu), 0, 0, getPos().width(), getPos().height());
}

void EqualizerWindow::setVolume(double volume01)
{
    if (!m_volume_slider) return;
    // Reflect only: the change came from outside (keyboard/MPRIS), so don't
    // echo it back through onVolumeChanged.
    m_suppress_volume_cb = true;
    m_volume_slider->setValue(volume01);
    m_suppress_volume_cb = false;
}

void EqualizerWindow::applyGains(const std::vector<double>& gains)
{
    // Driving the sliders routes through setOnChange, so the curve, readouts
    // and onBandChanged callback all update from one place.
    for (int i = 0; i < m_num && i < static_cast<int>(gains.size()); ++i)
        m_sliders[i]->setValue(gains[i]);
}

bool EqualizerWindow::saveToFile(const std::string& path) const
{
    std::ofstream f(System::pathFromUtf8(path), std::ios::out | std::ios::trunc);
    if (!f) return false;
    f << "# PsyMP3 Equalizer preset\n";
    f << "# <band>\t<gain dB>\n";
    for (int i = 0; i < m_num; ++i)
        f << (i < static_cast<int>(m_labels.size()) ? m_labels[i] : std::to_string(i))
          << '\t' << m_gains[i] << '\n';
    return static_cast<bool>(f);
}

bool EqualizerWindow::loadFromFile(const std::string& path)
{
    std::ifstream f(System::pathFromUtf8(path));
    if (!f) return false;
    std::vector<double> gains;
    std::string line;
    while (std::getline(f, line)) {
        // Trim leading whitespace.
        size_t s = line.find_first_not_of(" \t\r\n");
        if (s == std::string::npos) continue;
        if (line[s] == '#') continue;
        // The gain is the last whitespace-separated token on the line.
        std::istringstream iss(line);
        std::string tok, last;
        while (iss >> tok) last = tok;
        if (last.empty()) continue;
        try {
            // Require the whole token to be numeric: std::stod alone parses a
            // numeric prefix, so a hand-edited line holding only a band label
            // ("2.4k", "60") would be swallowed as a gain and shift every
            // following band by one.
            size_t consumed = 0;
            double v = std::stod(last, &consumed);
            if (consumed == last.size()) {
                gains.push_back(v);
            }
        } catch (const std::exception&) {
            // Skip unparseable lines rather than failing the whole load.
        }
    }
    if (gains.empty()) return false;
    applyGains(gains);
    return true;
}

std::string EqualizerWindow::userPresetPath(int slot)
{
    System::createStoragePath();
    return System::getStoragePath().to8Bit(true) + "/eq-user-" + std::to_string(slot) + ".psymp3eq";
}

void EqualizerWindow::saveUserPreset(int slot)
{
    if (slot < 1 || slot > kUserPresets) return;
    const bool ok = saveToFile(userPresetPath(slot));
    if (m_on_status)
        m_on_status(ok ? "Saved user preset " + std::to_string(slot)
                       : "Failed to save user preset " + std::to_string(slot));
}

void EqualizerWindow::loadUserPreset(int slot)
{
    if (slot < 1 || slot > kUserPresets) return;
    const bool ok = loadFromFile(userPresetPath(slot));
    if (m_on_status)
        m_on_status(ok ? "Loaded user preset " + std::to_string(slot)
                       : "User preset " + std::to_string(slot) + " is empty");
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
