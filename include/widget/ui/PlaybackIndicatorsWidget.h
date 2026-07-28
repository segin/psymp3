/*
 * PlaybackIndicatorsWidget.h - Repeat/Shuffle status indicators (Winamp-style).
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

// Small two-glyph status strip: a "repeat" loop icon (with a little "1" when in
// repeat-one mode) and a "shuffle" crossed-arrows icon. Each glyph lights up
// when its mode is active and is dimmed otherwise. Sits above the progress bar,
// to the right of the position text.
class PlaybackIndicatorsWidget : public PsyMP3::Widget::Foundation::DrawableWidget {
public:
    PlaybackIndicatorsWidget(int width, int height);
    ~PlaybackIndicatorsWidget() override = default;

    // Update the displayed state; only repaints when something changed.
    void setState(LoopMode loop, bool shuffle);

    // Install click handlers: clicking the repeat glyph invokes on_repeat,
    // clicking the shuffle glyph invokes on_shuffle.
    void setCallbacks(std::function<void()> on_repeat, std::function<void()> on_shuffle);

    // Clicking the left half toggles repeat, the right half toggles shuffle.
    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

protected:
    void draw(Surface& surface) override;

private:
    void drawRepeat(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b, bool show_one);
    void drawShuffle(Surface& s, int xoff, uint8_t r, uint8_t g, uint8_t b);

    LoopMode m_loop = LoopMode::None;
    bool m_shuffle = false;
    std::function<void()> m_on_repeat_click;
    std::function<void()> m_on_shuffle_click;

    static constexpr int kRepeatX = 0;   // left glyph origin
    static constexpr int kShuffleX = 21; // right glyph origin
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // PLAYBACKINDICATORSWIDGET_H
