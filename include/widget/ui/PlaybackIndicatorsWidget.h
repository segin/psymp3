/*
 * PlaybackIndicatorsWidget.h - Repeat/Shuffle/Power Saver status indicators (Winamp-style).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef PLAYBACKINDICATORSWIDGET_H
#define PLAYBACKINDICATORSWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// Small three-glyph status strip: a "repeat" loop icon (with a little "1" when
// in repeat-one mode), a "shuffle" crossed-arrows icon and a Power Saver
// battery-and-leaf icon. Repeat and shuffle light up white when on, Power
// Saver green, and each is dimmed otherwise. Sits above the progress bar, to
// the right of the position text.
class PlaybackIndicatorsWidget : public PsyMP3::Widget::Foundation::DrawableWidget {
public:
    PlaybackIndicatorsWidget(int width, int height);
    ~PlaybackIndicatorsWidget() override = default;

    // Update the displayed state; only repaints when something changed.
    void setState(LoopMode loop, bool shuffle, bool power_saver);

    // Install click handlers: clicking the repeat glyph invokes on_repeat,
    // the shuffle glyph on_shuffle, the Power Saver glyph on_power_saver.
    void setCallbacks(std::function<void()> on_repeat, std::function<void()> on_shuffle,
                      std::function<void()> on_power_saver);

    // A click acts on the glyph whose column it falls in.
    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

    /** @brief The width the three glyphs take up. */
    static constexpr int kWidth = 59;

protected:
    void draw(Surface& surface) override;

private:
    void drawRepeat(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b, bool show_one);
    void drawShuffle(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b);
    /** @brief Draws the Power Saver battery-and-leaf glyph. */
    void drawPowerSaver(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b);

    LoopMode m_loop = LoopMode::None;
    bool m_shuffle = false;
    bool m_power_saver = false;
    std::function<void()> m_on_repeat_click;
    std::function<void()> m_on_shuffle_click;
    std::function<void()> m_on_power_saver_click;

    // Glyph origins, 4 pixels apart: repeat 16 wide, shuffle 17, Power Saver 18.
    static constexpr int kRepeatX = 0;
    static constexpr int kShuffleX = 20;
    static constexpr int kPowerSaverX = 41;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // PLAYBACKINDICATORSWIDGET_H
