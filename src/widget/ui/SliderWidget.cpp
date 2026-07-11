/*
 * SliderWidget.cpp - Fader/slider widget implementation.
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
// A Win3.1-style raised bevel (light top/left, dark bottom/right).
void raisedBevel(::Surface& s, const Rect& r)
{
    int x2 = r.x() + r.width() - 1, y2 = r.y() + r.height() - 1;
    s.box(r.x(), r.y(), x2, y2, 192, 192, 192, 255);
    s.hline(r.x(), x2, r.y(), 255, 255, 255, 255);
    s.vline(r.x(), r.y(), y2, 255, 255, 255, 255);
    s.hline(r.x(), x2, y2, 128, 128, 128, 255);
    s.vline(x2, r.y(), y2, 128, 128, 128, 255);
}
} // namespace

SliderWidget::SliderWidget(int width, int height, double min_value, double max_value, double value,
                           SliderOrientation orientation)
    : Widget()
    , m_orientation(orientation)
    , m_min(min_value)
    , m_max(max_value)
    , m_value(value)
{
    if (m_max < m_min) std::swap(m_min, m_max);
    m_value = clamp(m_value);
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

double SliderWidget::clamp(double v) const
{
    if (v < m_min) return m_min;
    if (v > m_max) return m_max;
    return v;
}

int SliderWidget::axisLength() const
{
    return isVertical() ? getPos().height() : getPos().width();
}

int SliderWidget::thumbStartForValue(double v) const
{
    const int travel = std::max(0, axisLength() - kThumbLen);
    const double span = (m_max > m_min) ? (m_max - m_min) : 1.0;
    // Vertical: max at the top (frac 0). Horizontal: min at the left (frac 0).
    const double frac = isVertical() ? (m_max - v) / span : (v - m_min) / span;
    return static_cast<int>(std::round(frac * travel));
}

double SliderWidget::valueFromThumbStart(int start) const
{
    const int travel = std::max(1, axisLength() - kThumbLen);
    if (start < 0) start = 0;
    if (start > travel) start = travel;
    const double frac = static_cast<double>(start) / static_cast<double>(travel);
    return isVertical() ? (m_max - frac * (m_max - m_min))
                        : (m_min + frac * (m_max - m_min));
}

Rect SliderWidget::thumbRect() const
{
    Rect pos = getPos();
    const int start = thumbStartForValue(m_value);
    return isVertical() ? Rect(0, start, pos.width(), kThumbLen)
                        : Rect(start, 0, kThumbLen, pos.height());
}

void SliderWidget::setValue(double value)
{
    const double c = clamp(value);
    if (std::abs(c - m_value) < 1e-9) return;
    m_value = c;
    rebuildSurface();
    if (m_on_change) m_on_change(m_value);
}

bool SliderWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT || !hitTest(relative_x, relative_y))
        return false;

    const int coord = isVertical() ? relative_y : relative_x;
    Rect thumb = thumbRect();
    if (thumb.contains(relative_x, relative_y)) {
        m_drag_offset = coord - (isVertical() ? thumb.y() : thumb.x()); // grab where clicked
    } else {
        m_drag_offset = kThumbLen / 2;                                   // jump: centre on cursor
        setValue(valueFromThumbStart(coord - m_drag_offset));
    }
    m_dragging = true;
    captureMouse();
    rebuildSurface();
    return true;
}

bool SliderWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;
    if (!m_dragging)
        return hitTest(relative_x, relative_y);
    const int coord = isVertical() ? relative_y : relative_x;
    setValue(valueFromThumbStart(coord - m_drag_offset));
    return true;
}

bool SliderWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    (void)relative_x; (void)relative_y;
    if (event.button != SDL_BUTTON_LEFT || !m_dragging)
        return false;
    releaseMouse();
    m_dragging = false;
    rebuildSurface();
    return true;
}

void SliderWidget::rebuildSurface()
{
    Rect pos = getPos();
    const int w = pos.width(), h = pos.height();
    auto surface = std::make_unique<Surface>(w, h, true);
    surface->FillRect(surface->MapRGBA(0, 0, 0, 0)); // transparent; window bg shows

    const int travel = std::max(1, axisLength() - kThumbLen);
    const int lo = kThumbLen / 2;              // groove start along axis
    const int hi = lo + travel;                // groove end along axis

    if (isVertical()) {
        const int cx = w / 2;
        // Centre groove (recessed): dark left, light right.
        surface->vline(cx - 1, lo, hi, 128, 128, 128, 255);
        surface->vline(cx,     lo, hi, 255, 255, 255, 255);
        // Tick marks at 0/25/50/75/100%.
        for (int i = 0; i <= 4; ++i) {
            int ty = lo + (travel * i) / 4;
            int len = (i == 0 || i == 2 || i == 4) ? 4 : 2;
            surface->hline(cx - 3 - len, cx - 4, ty, 96, 96, 96, 255);
            surface->hline(cx + 5, cx + 4 + len, ty, 96, 96, 96, 255);
        }
    } else {
        const int cy = h / 2;
        surface->hline(lo, hi, cy - 1, 128, 128, 128, 255);
        surface->hline(lo, hi, cy,     255, 255, 255, 255);
        for (int i = 0; i <= 4; ++i) {
            int tx = lo + (travel * i) / 4;
            int len = (i == 0 || i == 2 || i == 4) ? 4 : 2;
            surface->vline(tx, cy - 3 - len, cy - 4, 96, 96, 96, 255);
            surface->vline(tx, cy + 5, cy + 4 + len, 96, 96, 96, 255);
        }
    }

    // Thumb: raised bevel with a centre grip line across the travel axis.
    Rect thumb = thumbRect();
    raisedBevel(*surface, thumb);
    if (isVertical()) {
        int gy = thumb.y() + thumb.height() / 2;
        surface->hline(thumb.x() + 2, thumb.x() + thumb.width() - 3, gy, 96, 96, 96, 255);
    } else {
        int gx = thumb.x() + thumb.width() / 2;
        surface->vline(gx, thumb.y() + 2, thumb.y() + thumb.height() - 3, 96, 96, 96, 255);
    }

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
