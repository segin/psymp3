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

CheckboxWidget* CheckboxWidget::s_focused_widget = nullptr;

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

CheckboxWidget::~CheckboxWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
}

void CheckboxWidget::takeFocus()
{
    if (s_focused_widget == this) {
        return;
    }
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
    s_focused_widget = this;
    rebuildSurface();
}

void CheckboxWidget::blur()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    if (m_key_pressed) {
        m_key_pressed = false;
        m_pressed = m_mouse_held;
    }
    rebuildSurface();
}

void CheckboxWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

bool CheckboxWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    CheckboxWidget* w = s_focused_widget;
    if (!w || !w->isEnabled() || keysym.sym != SDLK_SPACE) {
        return false;
    }
    // Key auto-repeat lands here again; the flag makes it a no-op.
    if (!w->m_key_pressed) {
        w->m_key_pressed = true;
        w->m_pressed = true;
        w->rebuildSurface();
    }
    return true;
}

bool CheckboxWidget::handleFocusedKeyUp(const SDL_keysym& keysym)
{
    CheckboxWidget* w = s_focused_widget;
    if (!w || keysym.sym != SDLK_SPACE || !w->m_key_pressed) {
        return false;
    }
    w->m_key_pressed = false;
    w->m_pressed = w->m_mouse_held;
    if (w->isEnabled()) {
        w->setChecked(!w->m_checked); // rebuilds
    } else {
        w->rebuildSurface();
    }
    return true;
}

bool CheckboxWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT) {
        return false;
    }

    if (!hitTest(relative_x, relative_y)) {
        return false;
    }

    takeFocus();
    m_pressed = true;
    m_mouse_held = true;
    captureMouse();
    rebuildSurface();
    return true;
}

bool CheckboxWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Keyed off the mouse's own flag, not m_pressed, which Space's release or
    // a blur can clear mid-press: the capture taken on the press must always
    // be released here.
    if (event.button != SDL_BUTTON_LEFT || !m_mouse_held) {
        return false;
    }

    releaseMouse();
    m_mouse_held = false;
    m_pressed = m_key_pressed;

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

    // The Windows 3.1 check box: a flat 13x13 box, a 1px black border round
    // white. Checked, an X runs corner to corner inside the border, its arms
    // crossing at the single centre pixel. Held down (mouse or Space), the
    // border thickens to 2px, covering the X's ends.
    const int box_size = 13;
    const int box_y = std::max(0, (pos.height() - box_size) / 2);
    const int last = box_size - 1;
    if (m_checked) {
        for (int i = 1; i < last; ++i) {
            surface->pixel(i, box_y + i, 0, 0, 0, 255);
            surface->pixel(i, box_y + last - i, 0, 0, 0, 255);
        }
    }
    surface->rectangle(0, box_y, last, box_y + last, 0, 0, 0, 255);
    if (m_pressed) {
        surface->rectangle(1, box_y + 1, last - 1, box_y + last - 1, 0, 0, 0, 255);
    }

    if (m_font && !m_text.isEmpty()) {
        // ClearType/LCD, pre-blended against the checkbox's white background.
        const uint8_t c = isEnabled() ? 0 : 128;
        auto text_surface = m_font->RenderLCD(m_text, c, c, c, 255, 255, 255);
        if (text_surface) {
            const int text_x = 18;
            const int text_y = std::max(0, (pos.height() - text_surface->height()) / 2);
            surface->Blit(*text_surface, Rect(text_x, text_y, text_surface->width(), text_surface->height()));

            // Keyboard focus: the classic dotted rectangle around the label.
            if (s_focused_widget == this) {
                const int x0 = text_x - 2;
                const int y0 = std::max(0, text_y - 1);
                const int x1 = std::min(pos.width() - 1, text_x + text_surface->width() + 1);
                const int y1 = std::min(pos.height() - 1, text_y + text_surface->height());
                for (int x = x0; x <= x1; ++x) {
                    if (((x + y0) & 1) == 0) surface->pixel(x, y0, 0, 0, 0, 255);
                    if (((x + y1) & 1) == 0) surface->pixel(x, y1, 0, 0, 0, 255);
                }
                for (int y = y0 + 1; y < y1; ++y) {
                    if (((x0 + y) & 1) == 0) surface->pixel(x0, y, 0, 0, 0, 255);
                    if (((x1 + y) & 1) == 0) surface->pixel(x1, y, 0, 0, 0, 255);
                }
            }
        }
    }

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
