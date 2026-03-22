/*
 * CheckboxWidget.cpp - Windows 3.1 style checkbox widget
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

void drawWin31SunkenFrame(::Surface& surface, int x, int y, int width, int height)
{
    surface.box(x, y, x + width - 1, y + height - 1, 255, 255, 255, 255);
    surface.hline(x, x + width - 1, y, 128, 128, 128, 255);
    surface.vline(x, y, y + height - 1, 128, 128, 128, 255);
    surface.hline(x, x + width - 1, y + height - 1, 255, 255, 255, 255);
    surface.vline(x + width - 1, y, y + height - 1, 255, 255, 255, 255);
    surface.hline(x + 1, x + width - 2, y + 1, 0, 0, 0, 255);
    surface.vline(x + 1, y + 1, y + height - 2, 0, 0, 0, 255);
}

} // namespace

CheckboxWidget::CheckboxWidget(int width, int height, Font* font, const TagLib::String& text, bool checked)
    : Widget()
    , m_font(font)
    , m_text(text)
    , m_checked(checked)
    , m_pressed(false)
    , m_hovered(false)
{
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

bool CheckboxWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT) {
        return false;
    }

    if (!hitTest(relative_x, relative_y)) {
        return false;
    }

    m_pressed = true;
    captureMouse();
    rebuildSurface();
    return true;
}

bool CheckboxWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button != SDL_BUTTON_LEFT || !m_pressed) {
        return false;
    }

    releaseMouse();
    m_pressed = false;

    if (hitTest(relative_x, relative_y) && isEnabled()) {
        setChecked(!m_checked);
    } else {
        rebuildSurface();
    }

    return true;
}

bool CheckboxWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;

    const bool hovered = hitTest(relative_x, relative_y);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        rebuildSurface();
    }

    return m_pressed || hovered;
}

void CheckboxWidget::setChecked(bool checked)
{
    if (m_checked == checked) {
        return;
    }

    m_checked = checked;
    rebuildSurface();
    if (m_on_toggle) {
        m_on_toggle(m_checked);
    }
}

void CheckboxWidget::setText(const TagLib::String& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    rebuildSurface();
}

void CheckboxWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));

    const int box_size = 13;
    const int box_y = std::max(0, (pos.height() - box_size) / 2);
    drawWin31SunkenFrame(*surface, 0, box_y, box_size, box_size);

    if (m_checked) {
        for (int i = 0; i < 7; ++i) {
            surface->pixel(3 + i, box_y + 6 + i / 2, 0, 0, 0, 255);
            surface->pixel(3 + i, box_y + 7 + i / 2, 0, 0, 0, 255);
        }
        for (int i = 0; i < 4; ++i) {
            surface->pixel(8 + i, box_y + 8 - i, 0, 0, 0, 255);
            surface->pixel(8 + i, box_y + 9 - i, 0, 0, 0, 255);
        }
    }

    if (m_font && !m_text.isEmpty()) {
        auto text_surface = m_font->Render(m_text, isEnabled() ? 0 : 128, isEnabled() ? 0 : 128, isEnabled() ? 0 : 128);
        if (text_surface) {
            const int text_x = 18;
            const int text_y = std::max(0, (pos.height() - text_surface->height()) / 2);
            surface->Blit(*text_surface, Rect(text_x, text_y, text_surface->width(), text_surface->height()));
        }
    }

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
