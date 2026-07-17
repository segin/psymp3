/*
 * ContextMenuWidget.cpp - Lightweight right-click popup menu
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

ContextMenuWidget::ContextMenuWidget(int width, int height, Core::Font* font)
    : DrawableWidget(width, height)
    , m_font(font)
{
}

void ContextMenuWidget::setEntries(std::vector<Entry> entries)
{
    m_entries = std::move(entries);

    // Popup width = widest label + padding on both sides.
    int max_w = 0;
    if (m_font && m_font->isValid()) {
        for (const auto& e : m_entries) {
            auto surf = m_font->RenderLCD(TagLib::String(e.label, TagLib::String::UTF8),
                                          0, 0, 0, 192, 192, 192);
            if (surf && surf->width() > max_w) max_w = surf->width();
        }
    }
    m_width_px = max_w + 2 * PAD + 2; // + the 1px borders
    if (m_width_px < 40) m_width_px = 40;
    invalidate();
}

void ContextMenuWidget::openAt(int x, int y)
{
    m_x = x;
    m_y = y;
    m_hover = -1;
    m_open = true;
    invalidate();
}

void ContextMenuWidget::close()
{
    if (m_open) {
        m_open = false;
        m_hover = -1;
        invalidate();
    }
}

Rect ContextMenuWidget::popupRect() const
{
    int w = m_width_px;
    int h = static_cast<int>(m_entries.size()) * ITEM_H + 2; // + 1px borders
    int x = m_x;
    int y = m_y;
    Rect me = getPos();
    if (x + w > me.width()) x = me.width() - w; // keep the popup on-screen
    if (y + h > me.height()) y = me.height() - h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    return Rect(x, y, w, h);
}

int ContextMenuWidget::itemAt(int x, int y) const
{
    Rect r = popupRect();
    if (x < r.x() || x >= r.x() + r.width() || y < r.y() || y >= r.y() + r.height()) {
        return -1;
    }
    int idx = (y - (r.y() + 1)) / ITEM_H;
    if (idx < 0 || idx >= static_cast<int>(m_entries.size())) {
        return -1;
    }
    return idx;
}

bool ContextMenuWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!m_open) {
        return false; // transparent while closed
    }
    // Any press resolves the menu: activate the item under it, or dismiss.
    int idx = (event.button == SDL_BUTTON_LEFT) ? itemAt(relative_x, relative_y) : -1;
    std::function<void()> action;
    if (idx >= 0 && m_entries[idx].enabled) {
        action = m_entries[idx].action;
    }
    close();
    if (action) {
        action();
    }
    return true;
}

bool ContextMenuWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;
    if (!m_open) {
        return false;
    }
    int idx = itemAt(relative_x, relative_y);
    if (idx >= 0 && !m_entries[idx].enabled) {
        idx = -1;
    }
    if (idx != m_hover) {
        m_hover = idx;
        invalidate();
    }
    return true;
}

bool ContextMenuWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    (void)event;
    (void)relative_x;
    (void)relative_y;
    // Consume releases while open (e.g. the right-button release that opened it)
    // so they don't fall through to the widgets underneath.
    return m_open;
}

void ContextMenuWidget::draw(Surface& surface)
{
    surface.FillRect(surface.MapRGBA(0, 0, 0, 0)); // transparent overlay
    if (!m_open || m_entries.empty()) {
        return;
    }

    Rect r = popupRect();
    const int x1 = r.x(), y1 = r.y();
    const int x2 = r.x() + r.width() - 1, y2 = r.y() + r.height() - 1;

    // Face + raised 3D border.
    surface.box(x1, y1, x2, y2, 192, 192, 192, 255);
    surface.hline(x1, x2, y1, 255, 255, 255, 255);
    surface.vline(x1, y1, y2, 255, 255, 255, 255);
    surface.hline(x1, x2, y2, 128, 128, 128, 255);
    surface.vline(x2, y1, y2, 128, 128, 128, 255);

    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        const Entry& e = m_entries[i];
        int iy = y1 + 1 + i * ITEM_H;
        bool hi = (i == m_hover) && e.enabled;
        if (hi) {
            surface.box(x1 + 1, iy, x2 - 1, iy + ITEM_H - 1, 0, 0, 128, 255);
        }
        if (m_font && m_font->isValid() && !e.label.empty()) {
            uint8_t fg = e.enabled ? (hi ? 255 : 0) : 128;
            uint8_t bg_r = hi ? 0 : 192, bg_g = hi ? 0 : 192, bg_b = hi ? 128 : 192;
            auto text = m_font->RenderLCD(TagLib::String(e.label, TagLib::String::UTF8),
                                          fg, fg, fg, bg_r, bg_g, bg_b);
            if (text && text->width() > 0) {
                int ty = iy + (ITEM_H - text->height()) / 2;
                surface.Blit(*text, Rect(x1 + PAD, ty, text->width(), text->height()));
            }
        }
    }
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
