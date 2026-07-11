/*
 * SliderWidget.h - Fader/slider widget (e.g. an EQ band gain).
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef SLIDERWIDGET_H
#define SLIDERWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

using PsyMP3::Widget::Foundation::Widget;

enum class SliderOrientation {
    Vertical,    // max at the top, min at the bottom (a fader)
    Horizontal   // min at the left, max at the right
};

// A draggable thumb over a centre groove, mapping the thumb position to a value
// in [min, max]. Clicking the track jumps the thumb to the cursor. Modelled on
// ScrollbarWidget's drag/capture pattern.
class SliderWidget : public Widget {
public:
    SliderWidget(int width, int height, double min_value, double max_value, double value,
                 SliderOrientation orientation = SliderOrientation::Vertical);
    ~SliderWidget() override = default;

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

    void   setValue(double value);           // clamps, redraws, fires onChange on change
    double getValue() const { return m_value; }
    void   setOnChange(std::function<void(double)> cb) { m_on_change = std::move(cb); }

    static constexpr int kThumbLen = 11;     // thumb size along the travel axis (px)

private:
    void   rebuildSurface();
    bool   isVertical() const { return m_orientation == SliderOrientation::Vertical; }
    int    axisLength() const;               // travel-axis length (height or width)
    Rect   thumbRect() const;                // thumb rect for the current value
    int    thumbStartForValue(double v) const;
    double valueFromThumbStart(int start) const;
    double clamp(double v) const;

    SliderOrientation m_orientation;
    double m_min;
    double m_max;
    double m_value;
    bool   m_dragging = false;
    int    m_drag_offset = 0;                 // cursor-to-thumb-start offset while dragging
    std::function<void(double)> m_on_change;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // SLIDERWIDGET_H
