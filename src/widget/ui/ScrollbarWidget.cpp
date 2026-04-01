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

void drawArrowGlyph(::Surface& surface, const Rect& rect, ButtonSymbol symbol)
{
    const int center_x = rect.x() + rect.width() / 2;
    const int center_y = rect.y() + rect.height() / 2;

    switch (symbol) {
        case ButtonSymbol::ScrollUp:
            surface.filledTriangle(center_x, center_y - 3, center_x + 4, center_y + 2, center_x - 4, center_y + 2, 0, 0, 0, 255);
            break;
        case ButtonSymbol::ScrollDown:
            surface.filledTriangle(center_x - 4, center_y - 2, center_x + 4, center_y - 2, center_x, center_y + 3, 0, 0, 0, 255);
            break;
        case ButtonSymbol::ScrollLeft:
            surface.filledTriangle(center_x + 2, center_y - 4, center_x + 2, center_y + 4, center_x - 3, center_y, 0, 0, 0, 255);
            break;
        case ButtonSymbol::ScrollRight:
            surface.filledTriangle(center_x - 2, center_y - 4, center_x - 2, center_y + 4, center_x + 3, center_y, 0, 0, 0, 255);
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

    Rect dec_arrow = getDecrementArrowRect();
    Rect inc_arrow = getIncrementArrowRect();
    Rect thumb = getThumbRect();

    drawWin31Button(*surface, dec_arrow, m_pressed && m_pressed_part == ScrollbarPart::DecrementArrow);
    drawWin31Button(*surface, inc_arrow, m_pressed && m_pressed_part == ScrollbarPart::IncrementArrow);

    if (m_orientation == ScrollbarOrientation::Vertical) {
        surface->box(1, dec_arrow.height(), pos.width() - 2, inc_arrow.y() - 1, 255, 255, 255, 255);
    } else {
        surface->box(dec_arrow.width(), 1, inc_arrow.x() - 1, pos.height() - 2, 255, 255, 255, 255);
    }

    drawWin31Button(*surface, thumb, m_dragging_thumb);

    drawArrowGlyph(*surface, dec_arrow,
                   m_orientation == ScrollbarOrientation::Vertical ? ButtonSymbol::ScrollUp : ButtonSymbol::ScrollLeft);
    drawArrowGlyph(*surface, inc_arrow,
                   m_orientation == ScrollbarOrientation::Vertical ? ButtonSymbol::ScrollDown : ButtonSymbol::ScrollRight);

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
