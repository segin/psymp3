/*
 * ScrollbarWidget.h - Windows 3.1 style scrollbar widget
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef SCROLLBARWIDGET_H
#define SCROLLBARWIDGET_H

namespace PsyMP3 {
namespace Widget {
namespace UI {

enum class ScrollbarOrientation {
    Vertical,
    Horizontal
};

class ScrollbarWidget : public Widget {
public:
    ScrollbarWidget(int width, int height, ScrollbarOrientation orientation = ScrollbarOrientation::Vertical);
    ~ScrollbarWidget() override = default;

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

    void setValue(double value);
    double getValue() const { return m_value; }
    void setOnChange(std::function<void(double)> callback) { m_on_change = callback; }

private:
    enum class ScrollbarPart {
        None,
        DecrementArrow,
        IncrementArrow,
        Thumb,
        TrackBeforeThumb,
        TrackAfterThumb
    };

    void rebuildSurface();
    ScrollbarPart hitTestPart(int relative_x, int relative_y) const;
    Rect getDecrementArrowRect() const;
    Rect getIncrementArrowRect() const;
    Rect getThumbRect() const;
    void notifyChange();
    double clampValue(double value) const;

    ScrollbarOrientation m_orientation;
    double m_value;
    bool m_pressed;
    bool m_dragging_thumb;
    ScrollbarPart m_pressed_part;
    int m_drag_offset;
    std::function<void(double)> m_on_change;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // SCROLLBARWIDGET_H
