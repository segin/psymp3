/*
 * RadioButtonWidget.cpp - Windows 3.1 style radio (option) button
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

RadioButtonWidget* RadioButtonWidget::s_focused_widget = nullptr;

namespace {

constexpr int kSize = 13;

// The 13x13 ring, pixel-counted from Windows 3.1; '#' is black.
const char* const kRing[kSize] = {
    ".....###.....",
    "...##...##...",
    "..#.......#..",
    ".#.........#.",
    ".#.........#.",
    "#...........#",
    "#...........#",
    "#...........#",
    ".#.........#.",
    ".#.........#.",
    "..#.......#..",
    "...##...##...",
    ".....###.....",
};
// Drawn inside the ring while held down, doubling its thickness.
const char* const kInnerRing[kSize] = {
    ".............",
    ".....###.....",
    "...##...##...",
    "..#.......#..",
    "..#.......#..",
    ".#.........#.",
    ".#.........#.",
    ".#.........#.",
    "..#.......#..",
    "..#.......#..",
    "...##...##...",
    ".....###.....",
    ".............",
};
// The bullet, pixel-counted from Windows 3.1.
const char* const kBullet[kSize] = {
    ".............",
    ".............",
    ".............",
    ".....###.....",
    "....#####....",
    "...#######...",
    "...#######...",
    "...#######...",
    "....#####....",
    ".....###.....",
    ".............",
    ".............",
    ".............",
};

void drawPattern(::Surface& surface, const char* const rows[kSize], int y0, uint8_t shade)
{
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            if (rows[y][x] == '#') {
                surface.pixel(x, y0 + y, shade, shade, shade, 255);
            }
        }
    }
}

} // namespace

RadioButtonWidget::RadioButtonWidget(int width, int height, Font* font, const TagLib::String& text)
    : Widget()
    , m_font(font)
    , m_text(text)
{
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

RadioButtonWidget::~RadioButtonWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    if (m_group) {
        auto& members = m_group->members;
        members.erase(std::remove(members.begin(), members.end(), this), members.end());
    }
}

void RadioButtonWidget::makeGroup(const std::vector<RadioButtonWidget*>& buttons)
{
    auto group = std::make_shared<Group>();
    for (RadioButtonWidget* b : buttons) {
        if (b) {
            group->members.push_back(b);
            b->m_group = group;
        }
    }
}

void RadioButtonWidget::setSelected()
{
    if (m_group) {
        for (RadioButtonWidget* b : m_group->members) {
            if (b != this && b->m_selected) {
                b->m_selected = false;
                b->rebuildSurface();
            }
        }
    }
    if (!m_selected) {
        m_selected = true;
        rebuildSurface();
    }
}

void RadioButtonWidget::choose()
{
    if (m_selected) {
        return; // already the choice: nothing changes
    }
    setSelected();
    if (m_on_select) {
        auto on_select = m_on_select; // the callback may replace it
        on_select();
    }
}

void RadioButtonWidget::takeFocus()
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

void RadioButtonWidget::blur()
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

void RadioButtonWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

bool RadioButtonWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    RadioButtonWidget* w = s_focused_widget;
    if (!w || !w->isEnabled()) {
        return false;
    }
    if (keysym.sym == SDLK_SPACE) {
        // Key auto-repeat lands here again; the flag makes it a no-op.
        if (!w->m_key_pressed) {
            w->m_key_pressed = true;
            w->m_pressed = true;
            w->rebuildSurface();
        }
        return true;
    }
    // The arrow keys move the selection, and the focus, through the group.
    int step = 0;
    if (keysym.sym == SDLK_UP || keysym.sym == SDLK_LEFT) {
        step = -1;
    } else if (keysym.sym == SDLK_DOWN || keysym.sym == SDLK_RIGHT) {
        step = 1;
    }
    if (step == 0 || (keysym.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT)) != 0) {
        // As the combo box does: Alt chords (mnemonics), Enter (the default
        // button), Tab and the function keys go on; Escape drops focus rather
        // than reach the quit key; the rest is swallowed, so the global
        // shortcuts (Q quits, N skips, ...) don't fire while it has focus.
        if ((keysym.mod & SDL_KMOD_ALT) != 0 || keysym.sym == SDLK_RETURN ||
            keysym.sym == SDLK_KP_ENTER || keysym.sym == SDLK_TAB ||
            (keysym.sym >= SDLK_F1 && keysym.sym <= SDLK_F12)) {
            return false;
        }
        if (keysym.sym == SDLK_ESCAPE) {
            w->blur();
        }
        return true;
    }
    if (!w->m_group) {
        return true;
    }
    const auto& members = w->m_group->members;
    const int n = static_cast<int>(members.size());
    auto it = std::find(members.begin(), members.end(), w);
    if (it == members.end() || n < 2) {
        return true;
    }
    int index = static_cast<int>(it - members.begin());
    // Skip disabled buttons, wrapping round the group.
    for (int k = 0; k < n - 1; ++k) {
        index = (index + step + n) % n;
        RadioButtonWidget* next = members[static_cast<size_t>(index)];
        if (next->isEnabled()) {
            next->takeFocus();
            next->choose();
            break;
        }
    }
    return true;
}

bool RadioButtonWidget::handleFocusedKeyUp(const SDL_keysym& keysym)
{
    RadioButtonWidget* w = s_focused_widget;
    if (!w || keysym.sym != SDLK_SPACE || !w->m_key_pressed) {
        return false;
    }
    w->m_key_pressed = false;
    w->m_pressed = w->m_mouse_held;
    w->rebuildSurface();
    if (w->isEnabled()) {
        w->choose();
    }
    return true;
}

bool RadioButtonWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT || !hitTest(relative_x, relative_y)) {
        return false;
    }
    takeFocus();
    m_pressed = true;
    m_mouse_held = true;
    captureMouse();
    rebuildSurface();
    return true;
}

bool RadioButtonWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
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
    rebuildSurface();
    // Released off the button, the press is cancelled.
    if (hitTest(relative_x, relative_y) && isEnabled()) {
        choose();
    }
    return true;
}

bool RadioButtonWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;
    return m_pressed || hitTest(relative_x, relative_y);
}

void RadioButtonWidget::rebuildSurface()
{
    Rect pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));

    const uint8_t shade = isEnabled() ? 0 : 128;
    const int ring_y = std::max(0, (pos.height() - kSize) / 2);
    drawPattern(*surface, kRing, ring_y, shade);
    if (m_pressed) {
        drawPattern(*surface, kInnerRing, ring_y, shade);
    }
    if (m_selected) {
        drawPattern(*surface, kBullet, ring_y, shade);
    }

    if (m_font && !m_text.isEmpty()) {
        // ClearType/LCD, pre-blended against the white background.
        auto text_surface = m_font->RenderLCD(m_text, shade, shade, shade, 255, 255, 255);
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
