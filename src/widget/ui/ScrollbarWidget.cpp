/*
 * ScrollbarWidget.cpp - Windows 3.1 style scrollbar widget
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

void drawWin31Button(::Surface& surface, const Rect& rect, bool pressed)
{
    surface.box(rect.x(), rect.y(), rect.x() + rect.width() - 1, rect.y() + rect.height() - 1, 192, 192, 192, 255);
    if (pressed) {
        surface.hline(rect.x(), rect.x() + rect.width() - 1, rect.y(), 128, 128, 128, 255);
        surface.vline(rect.x(), rect.y(), rect.y() + rect.height() - 1, 128, 128, 128, 255);
        surface.hline(rect.x(), rect.x() + rect.width() - 1, rect.y() + rect.height() - 1, 255, 255, 255, 255);
        surface.vline(rect.x() + rect.width() - 1, rect.y(), rect.y() + rect.height() - 1, 255, 255, 255, 255);
    } else {
        surface.hline(rect.x(), rect.x() + rect.width() - 2, rect.y(), 255, 255, 255, 255);
        surface.vline(rect.x(), rect.y(), rect.y() + rect.height() - 2, 255, 255, 255, 255);
        surface.hline(rect.x(), rect.x() + rect.width() - 1, rect.y() + rect.height() - 1, 128, 128, 128, 255);
        surface.vline(rect.x() + rect.width() - 1, rect.y(), rect.y() + rect.height() - 1, 128, 128, 128, 255);
    }
}

void drawArrowGlyph(::Surface& surface, const Rect& rect, ButtonSymbol symbol, uint8_t shade = 0)
{
    // Pixel-drawn triangles (one row/column narrower per step from the base to a
    // 1px apex), matching the crisp min/max glyphs on the window frame buttons.
    // SDL_gfx's filledTriangle produced squatter, softer arrows that looked out
    // of place next to them.
    const int cx = rect.x() + rect.width() / 2;
    const int cy = rect.y() + rect.height() / 2;
    const uint8_t c = shade; // 0 = black (enabled), 128 = grey (disabled)

    switch (symbol) {
        case ButtonSymbol::ScrollUp: // apex up, 7px base
            surface.pixel(cx, cy - 2, c, c, c, 255);
            surface.hline(cx - 1, cx + 1, cy - 1, c, c, c, 255);
            surface.hline(cx - 2, cx + 2, cy,     c, c, c, 255);
            surface.hline(cx - 3, cx + 3, cy + 1, c, c, c, 255);
            break;
        case ButtonSymbol::ScrollDown: // apex down, 7px base
            surface.hline(cx - 3, cx + 3, cy - 1, c, c, c, 255);
            surface.hline(cx - 2, cx + 2, cy,     c, c, c, 255);
            surface.hline(cx - 1, cx + 1, cy + 1, c, c, c, 255);
            surface.pixel(cx, cy + 2, c, c, c, 255);
            break;
        case ButtonSymbol::ScrollLeft: // apex left, 7px base
            surface.pixel(cx - 2, cy, c, c, c, 255);
            surface.vline(cx - 1, cy - 1, cy + 1, c, c, c, 255);
            surface.vline(cx,     cy - 2, cy + 2, c, c, c, 255);
            surface.vline(cx + 1, cy - 3, cy + 3, c, c, c, 255);
            break;
        case ButtonSymbol::ScrollRight: // apex right, 7px base
            surface.vline(cx - 1, cy - 3, cy + 3, c, c, c, 255);
            surface.vline(cx,     cy - 2, cy + 2, c, c, c, 255);
            surface.vline(cx + 1, cy - 1, cy + 1, c, c, c, 255);
            surface.pixel(cx + 2, cy, c, c, c, 255);
            break;
        default:
            break;
    }
}

} // namespace

ScrollbarWidget::ScrollbarWidget(int width, int height, ScrollbarOrientation orientation)
    : Widget()
    , m_orientation(orientation)
    , m_value(0.5)
    , m_pressed(false)
    , m_dragging_thumb(false)
    , m_pressed_part(ScrollbarPart::None)
    , m_drag_offset(0)
{
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

bool ScrollbarWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT || !hitTest(relative_x, relative_y)) {
        return false;
    }

    m_pressed = true;
    m_pressed_part = hitTestPart(relative_x, relative_y);

    switch (m_pressed_part) {
        case ScrollbarPart::DecrementArrow:
            setValue(m_value - 0.08);
            break;
        case ScrollbarPart::IncrementArrow:
            setValue(m_value + 0.08);
            break;
        case ScrollbarPart::TrackBeforeThumb:
            setValue(m_value - 0.2);
            break;
        case ScrollbarPart::TrackAfterThumb:
            setValue(m_value + 0.2);
            break;
        case ScrollbarPart::Thumb:
        {
            m_dragging_thumb = true;
            captureMouse();
            Rect thumb = getThumbRect();
            m_drag_offset = (m_orientation == ScrollbarOrientation::Vertical)
                ? (relative_y - thumb.y())
                : (relative_x - thumb.x());
            break;
        }
        case ScrollbarPart::None:
            break;
    }

    rebuildSurface();
    return true;
}

bool ScrollbarWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;

    if (!m_dragging_thumb) {
        return hitTest(relative_x, relative_y);
    }

    Rect pos = getPos();
    const int arrow_extent = (m_orientation == ScrollbarOrientation::Vertical) ? pos.width() : pos.height();
    const int thumb_extent = (m_orientation == ScrollbarOrientation::Vertical)
        ? std::max(pos.width() - 2, 12)
        : std::max(pos.height() - 2, 12);
    const int travel = ((m_orientation == ScrollbarOrientation::Vertical) ? pos.height() : pos.width()) - (2 * arrow_extent) - thumb_extent;

    if (travel <= 0) {
        return true;
    }

    const int coordinate = (m_orientation == ScrollbarOrientation::Vertical) ? relative_y : relative_x;
    const int thumb_origin = coordinate - m_drag_offset;
    const double normalized = static_cast<double>(thumb_origin - arrow_extent) / static_cast<double>(travel);
    setValue(normalized);
    return true;
}

bool ScrollbarWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    (void)relative_x;
    (void)relative_y;

    if (event.button != SDL_BUTTON_LEFT || !m_pressed) {
        return false;
    }

    if (m_dragging_thumb) {
        releaseMouse();
    }

    m_pressed = false;
    m_dragging_thumb = false;
    m_pressed_part = ScrollbarPart::None;
    rebuildSurface();
    return true;
}

void ScrollbarWidget::setGeometry(const Rect& bounds)
{
    setPos(bounds);
    rebuildSurface();
}

void ScrollbarWidget::setEnabled(bool enabled)
{
    if (enabled == isEnabled()) {
        return;
    }
    Widget::setEnabled(enabled);
    rebuildSurface(); // switch between the normal and greyed appearance
}

void ScrollbarWidget::setValue(double value)
{
    const double clamped = clampValue(value);
    if (std::abs(clamped - m_value) < 0.0001) {
        return;
    }

    m_value = clamped;
    rebuildSurface();
    notifyChange();
}

ScrollbarWidget::ScrollbarPart ScrollbarWidget::hitTestPart(int relative_x, int relative_y) const
{
    if (getDecrementArrowRect().contains(relative_x, relative_y)) {
        return ScrollbarPart::DecrementArrow;
    }
    if (getIncrementArrowRect().contains(relative_x, relative_y)) {
        return ScrollbarPart::IncrementArrow;
    }

    Rect thumb = getThumbRect();
    if (thumb.contains(relative_x, relative_y)) {
        return ScrollbarPart::Thumb;
    }

    if (m_orientation == ScrollbarOrientation::Vertical) {
        return (relative_y < thumb.y()) ? ScrollbarPart::TrackBeforeThumb : ScrollbarPart::TrackAfterThumb;
    }
    return (relative_x < thumb.x()) ? ScrollbarPart::TrackBeforeThumb : ScrollbarPart::TrackAfterThumb;
}

Rect ScrollbarWidget::getDecrementArrowRect() const
{
    Rect pos = getPos();
    const int extent = (m_orientation == ScrollbarOrientation::Vertical) ? pos.width() : pos.height();
    return (m_orientation == ScrollbarOrientation::Vertical)
        ? Rect(0, 0, pos.width(), extent)
        : Rect(0, 0, extent, pos.height());
}

Rect ScrollbarWidget::getIncrementArrowRect() const
{
    Rect pos = getPos();
    const int extent = (m_orientation == ScrollbarOrientation::Vertical) ? pos.width() : pos.height();
    return (m_orientation == ScrollbarOrientation::Vertical)
        ? Rect(0, pos.height() - extent, pos.width(), extent)
        : Rect(pos.width() - extent, 0, extent, pos.height());
}

Rect ScrollbarWidget::getThumbRect() const
{
    Rect pos = getPos();
    const int arrow_extent = (m_orientation == ScrollbarOrientation::Vertical) ? pos.width() : pos.height();
    const int thumb_extent = (m_orientation == ScrollbarOrientation::Vertical)
        ? std::max(pos.width() - 2, 12)
        : std::max(pos.height() - 2, 12);
    const int track_extent = ((m_orientation == ScrollbarOrientation::Vertical) ? pos.height() : pos.width()) - (2 * arrow_extent);
    const int travel = std::max(0, track_extent - thumb_extent);
    const int thumb_offset = arrow_extent + static_cast<int>(std::round(m_value * travel));

    return (m_orientation == ScrollbarOrientation::Vertical)
        ? Rect(1, thumb_offset, pos.width() - 2, thumb_extent)
        : Rect(thumb_offset, 1, thumb_extent, pos.height() - 2);
}

void ScrollbarWidget::notifyChange()
{
    if (m_on_change) {
        m_on_change(m_value);
    }
}

double ScrollbarWidget::clampValue(double value) const
{
    return std::max(0.0, std::min(1.0, value));
}

void ScrollbarWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));

    const bool enabled = isEnabled();

    Rect dec_arrow = getDecrementArrowRect();
    Rect inc_arrow = getIncrementArrowRect();
    Rect thumb = getThumbRect();

    // Arrow buttons keep their bevel but only show a pressed state when enabled.
    drawWin31Button(*surface, dec_arrow, enabled && m_pressed && m_pressed_part == ScrollbarPart::DecrementArrow);
    drawWin31Button(*surface, inc_arrow, enabled && m_pressed && m_pressed_part == ScrollbarPart::IncrementArrow);

    // Track: white when enabled, flat grey when disabled.
    const uint8_t track = enabled ? 255 : 192;
    if (m_orientation == ScrollbarOrientation::Vertical) {
        surface->box(1, dec_arrow.height(), pos.width() - 2, inc_arrow.y() - 1, track, track, track, 255);
    } else {
        surface->box(dec_arrow.width(), 1, inc_arrow.x() - 1, pos.height() - 2, track, track, track, 255);
    }

    // Disabled scrollbars have no thumb and grey (not black) arrow glyphs — the
    // classic "nothing to scroll" look.
    if (enabled) {
        drawWin31Button(*surface, thumb, m_dragging_thumb);
    }

    const uint8_t glyph_shade = enabled ? 0 : 128;
    drawArrowGlyph(*surface, dec_arrow,
                   m_orientation == ScrollbarOrientation::Vertical ? ButtonSymbol::ScrollUp : ButtonSymbol::ScrollLeft,
                   glyph_shade);
    drawArrowGlyph(*surface, inc_arrow,
                   m_orientation == ScrollbarOrientation::Vertical ? ButtonSymbol::ScrollDown : ButtonSymbol::ScrollRight,
                   glyph_shade);

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
