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
    // Overridden to drive track-hold auto-repeat: it fires once per rendered
    // frame (like the fading widgets), paging toward the held cursor on a timer.
    void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos) override;

    void setValue(double value);
    double getValue() const { return m_value; }
    // Repaint in the disabled (greyed, thumbless) style when turned off.
    void setEnabled(bool enabled) override;
    // Set how far an arrow click (line) and a track click/hold (page) move, as
    // fractions of the 0..1 range. An owner that maps the value onto N scroll
    // positions passes 1/N and (page rows)/N so a page click moves one visible
    // page of content regardless of list size. Clamped to (0, 1].
    void setSteps(double line_step, double page_step);
    void setOnChange(std::function<void(double)> callback) { m_on_change = callback; }

    // Reposition and resize the scrollbar in one shot, rebuilding its surface so
    // the arrows/thumb track the new geometry. Use this instead of setPos() when
    // a container re-lays out a scrollbar in response to a resize.
    void setGeometry(const Rect& bounds);

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
    // The value that would centre the thumb under (relative_x, relative_y).
    double valueAtCoordinate(int relative_x, int relative_y) const;
    // Page one step toward the held cursor, clamped so the thumb never moves
    // past it (this is what makes paging stop once the thumb reaches the cursor).
    void pageTowardCursor();
    // Called each frame while a track hold is active; pages on the repeat timer.
    void tickAutoRepeat();

    ScrollbarOrientation m_orientation;
    double m_value;
    bool m_pressed;
    bool m_dragging_thumb;
    ScrollbarPart m_pressed_part;
    int m_drag_offset;
    std::function<void(double)> m_on_change;

    // Movement per arrow click (line) and per track click/hold (page), as
    // fractions of the range. Defaults suit a bare scrollbar; an owner that
    // knows its content sets them via setSteps().
    double m_line_step = 0.08;
    double m_page_step = 0.2;

    // Track press-and-hold auto-repeat state.
    bool m_track_repeating = false;   // holding on the track, paging toward cursor
    int m_track_x = 0;                // last known cursor position (widget-relative)
    int m_track_y = 0;
    Uint32 m_next_repeat_ms = 0;      // SDL_GetTicks() time of the next page step
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // SCROLLBARWIDGET_H
