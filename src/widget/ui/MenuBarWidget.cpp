/*
 * MenuBarWidget.cpp - In-app (SDL-drawn) menu bar implementation.
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
// Win9x-ish palette.
constexpr SDL_Color kBar      = {192, 192, 192, 255};
constexpr SDL_Color kFace     = {198, 198, 198, 255};
constexpr SDL_Color kLight    = {255, 255, 255, 255};
constexpr SDL_Color kShadow   = {110, 110, 110, 255};
constexpr SDL_Color kText     = {0, 0, 0, 255};
constexpr SDL_Color kHiBg     = {0, 0, 128, 255};
constexpr SDL_Color kHiText   = {255, 255, 255, 255};
}

MenuBarWidget::MenuBarWidget(int width, int height, Font* font)
    : m_font(font)
{
    setPos(Rect(0, 0, width, height));
    rebuild();
}

int MenuBarWidget::textWidth(const std::string& s) const
{
    auto it = m_text_w.find(s);
    if (it != m_text_w.end()) return it->second;
    int w = 0;
    if (m_font) {
        if (auto surf = m_font->Render(TagLib::String(s, TagLib::String::UTF8), 0, 0, 0)) {
            w = surf->width();
        }
    }
    m_text_w[s] = w;
    return w;
}

std::unique_ptr<Surface> MenuBarWidget::renderText(const std::string& s, uint8_t r, uint8_t g, uint8_t b) const
{
    if (!m_font || s.empty()) return nullptr;
    return m_font->Render(TagLib::String(s, TagLib::String::UTF8), r, g, b);
}

void MenuBarWidget::addMenu(std::string name, std::vector<Item> items)
{
    Menu m;
    m.name = std::move(name);
    m.items = std::move(items);
    int x = 0;
    for (const auto& e : m_menus) x = e.bar_x + e.bar_w;
    m.bar_x = x;
    m.bar_w = textWidth(m.name) + BAR_PAD * 2;
    m_menus.push_back(std::move(m));
    rebuild();
}

void MenuBarWidget::closeMenu()
{
    if (m_open >= 0 || m_open_sub >= 0) {
        m_open = -1;
        m_hover = -1;
        m_open_sub = -1;
        m_hover_sub = -1;
        rebuild();
    }
}

int MenuBarWidget::itemTopY(const std::vector<Item>& items, int i) const
{
    int y = 1; // top border
    for (int k = 0; k < i && k < static_cast<int>(items.size()); ++k)
        y += items[k].separator ? SEP_H : ITEM_H;
    return y;
}

int MenuBarWidget::popupHeight(const std::vector<Item>& items) const
{
    int h = 2; // top+bottom border
    for (const auto& it : items) h += it.separator ? SEP_H : ITEM_H;
    return h;
}

int MenuBarWidget::popupWidth(const std::vector<Item>& items) const
{
    int maxw = 0;
    for (const auto& it : items) {
        if (it.separator) continue;
        maxw = std::max(maxw, textWidth(it.label));
    }
    return CHECK_COL + maxw + ARROW_COL + 4;
}

Rect MenuBarWidget::dropdownRect(const Menu& m) const
{
    int w = popupWidth(m.items);
    int h = popupHeight(m.items);
    int x = m.bar_x;
    if (x + w > getPos().width()) x = getPos().width() - w; // keep on-screen
    if (x < 0) x = 0;
    return Rect(x, BAR_H, w, h);
}

Rect MenuBarWidget::submenuRect(const Menu& m, int item_index) const
{
    Rect dd = dropdownRect(m);
    const auto& sub = m.items[item_index].submenu;
    int w = popupWidth(sub);
    int h = popupHeight(sub);
    int x = dd.x() + dd.width() - 1;
    int y = dd.y() + itemTopY(m.items, item_index) - 1;
    if (x + w > getPos().width()) x = dd.x() - w + 1; // flip left if off-screen
    if (x < 0) x = 0;
    if (y + h > getPos().height()) y = getPos().height() - h;
    if (y < 0) y = 0;
    return Rect(x, y, w, h);
}

int MenuBarWidget::barHitTest(int x, int y) const
{
    if (y < 0 || y >= BAR_H) return -1;
    for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
        const Menu& m = m_menus[i];
        if (x >= m.bar_x && x < m.bar_x + m.bar_w) return i;
    }
    return -1;
}

int MenuBarWidget::itemAt(const std::vector<Item>& items, const Rect& box, int x, int y) const
{
    if (x < box.x() || x >= box.x() + box.width() || y < box.y() || y >= box.y() + box.height())
        return -1;
    int local_y = y - box.y();
    int cy = 1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        int h = items[i].separator ? SEP_H : ITEM_H;
        if (local_y >= cy && local_y < cy + h)
            return items[i].separator ? -1 : i;
        cy += h;
    }
    return -1;
}

// ---- Drawing ------------------------------------------------------------

static void raisedBorder(Surface& s, const Rect& r, SDL_Color light, SDL_Color shadow)
{
    uint32_t lc = s.MapRGB(light.r, light.g, light.b);
    uint32_t sc = s.MapRGB(shadow.r, shadow.g, shadow.b);
    int x1 = r.x(), y1 = r.y(), x2 = r.x() + r.width() - 1, y2 = r.y() + r.height() - 1;
    s.hline(x1, x2, y1, lc);
    s.vline(x1, y1, y2, lc);
    s.hline(x1, x2, y2, sc);
    s.vline(x2, y1, y2, sc);
}

void MenuBarWidget::rebuild()
{
    const int W = getPos().width();
    const int H = getPos().height();
    auto surf = std::make_unique<Surface>(W, H, true);
    surf->FillRect(surf->MapRGBA(0, 0, 0, 0)); // transparent everywhere

    // --- the bar ---
    surf->box(0, 0, W - 1, BAR_H - 1, kBar.r, kBar.g, kBar.b, 255);
    surf->hline(0, W - 1, BAR_H - 1, kShadow.r, kShadow.g, kShadow.b, 255);
    for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
        const Menu& m = m_menus[i];
        bool open = (i == m_open);
        if (open)
            surf->box(m.bar_x, 0, m.bar_x + m.bar_w - 1, BAR_H - 2, kHiBg.r, kHiBg.g, kHiBg.b, 255);
        SDL_Color tc = open ? kHiText : kText;
        if (auto t = renderText(m.name, tc.r, tc.g, tc.b)) {
            int ty = (BAR_H - t->height()) / 2;
            surf->Blit(*t, Rect(m.bar_x + BAR_PAD, ty, t->width(), t->height()));
        }
    }

    // --- a dropdown popup ---
    auto drawPopup = [&](const std::vector<Item>& items, const Rect& box, int hover) {
        surf->box(box.x(), box.y(), box.x() + box.width() - 1, box.y() + box.height() - 1,
                  kFace.r, kFace.g, kFace.b, 255);
        raisedBorder(*surf, box, kLight, kShadow);
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const Item& it = items[i];
            int ry = box.y() + itemTopY(items, i);
            if (it.separator) {
                surf->hline(box.x() + 2, box.x() + box.width() - 3, ry + SEP_H / 2,
                            kShadow.r, kShadow.g, kShadow.b, 255);
                continue;
            }
            bool hi = (i == hover);
            if (hi)
                surf->box(box.x() + 1, ry, box.x() + box.width() - 2, ry + ITEM_H - 1,
                          kHiBg.r, kHiBg.g, kHiBg.b, 255);
            SDL_Color tc = hi ? kHiText : kText;
            // radio dot
            if (it.checked && it.checked()) {
                int cx = box.x() + 5, cyd = ry + ITEM_H / 2 - 2;
                surf->box(cx, cyd, cx + 3, cyd + 3, tc.r, tc.g, tc.b, 255);
            }
            if (auto t = renderText(it.label, tc.r, tc.g, tc.b)) {
                int ty = ry + (ITEM_H - t->height()) / 2;
                surf->Blit(*t, Rect(box.x() + CHECK_COL, ty, t->width(), t->height()));
            }
            // submenu arrow
            if (!it.submenu.empty()) {
                int ax = box.x() + box.width() - ARROW_COL + 3;
                int ay = ry + ITEM_H / 2;
                for (int k = 0; k < 4; ++k)
                    surf->vline(ax + k, ay - (3 - k), ay + (3 - k), tc.r, tc.g, tc.b, 255);
            }
        }
    };

    if (m_open >= 0 && m_open < static_cast<int>(m_menus.size())) {
        const Menu& m = m_menus[m_open];
        Rect dd = dropdownRect(m);
        drawPopup(m.items, dd, m_hover);
        if (m_open_sub >= 0 && m_open_sub < static_cast<int>(m.items.size())
            && !m.items[m_open_sub].submenu.empty()) {
            drawPopup(m.items[m_open_sub].submenu, submenuRect(m, m_open_sub), m_hover_sub);
        }
    }

    setSurface(std::move(surf));
    invalidate();
}

// ---- Events -------------------------------------------------------------

bool MenuBarWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int x, int y)
{
    if (event.button != SDL_BUTTON_LEFT) {
        if (isOpen()) { closeMenu(); return true; }
        return false;
    }

    int bar = barHitTest(x, y);
    if (bar >= 0) {
        // toggle the clicked top-level menu
        if (m_open == bar) { closeMenu(); }
        else { m_open = bar; m_hover = -1; m_open_sub = -1; m_hover_sub = -1; rebuild(); }
        return true;
    }

    if (m_open >= 0) {
        const Menu& m = m_menus[m_open];
        Rect dd = dropdownRect(m);
        // submenu first (it overlaps to the right)
        if (m_open_sub >= 0 && !m.items[m_open_sub].submenu.empty()) {
            Rect sr = submenuRect(m, m_open_sub);
            int si = itemAt(m.items[m_open_sub].submenu, sr, x, y);
            if (si >= 0) {
                auto act = m.items[m_open_sub].submenu[si].action;
                closeMenu();
                if (act) act();
                return true;
            }
        }
        int di = itemAt(m.items, dd, x, y);
        if (di >= 0) {
            const Item& it = m.items[di];
            if (!it.submenu.empty()) {
                m_open_sub = (m_open_sub == di) ? -1 : di;
                m_hover_sub = -1;
                rebuild();
                return true;
            }
            auto act = it.action;
            closeMenu();
            if (act) act();
            return true;
        }
        // click outside any popup -> dismiss (and consume this click)
        closeMenu();
        return true;
    }

    return false; // nothing open, click not on the bar -> pass through
}

bool MenuBarWidget::handleMouseMotion(const SDL_MouseMotionEvent&, int x, int y)
{
    if (m_open < 0) return false; // let hover pass through to widgets beneath

    const Menu& m = m_menus[m_open];

    // switching between top-level menus by hovering the bar while open
    int bar = barHitTest(x, y);
    if (bar >= 0 && bar != m_open) {
        m_open = bar; m_hover = -1; m_open_sub = -1; m_hover_sub = -1; rebuild();
        return true;
    }

    int new_hover = itemAt(m.items, dropdownRect(m), x, y);
    int new_hover_sub = -1;
    if (m_open_sub >= 0 && !m.items[m_open_sub].submenu.empty())
        new_hover_sub = itemAt(m.items[m_open_sub].submenu, submenuRect(m, m_open_sub), x, y);

    // hovering a submenu-parent item expands it
    int new_open_sub = m_open_sub;
    if (new_hover >= 0 && !m.items[new_hover].submenu.empty())
        new_open_sub = new_hover;
    else if (new_hover >= 0 && new_hover_sub < 0)
        new_open_sub = -1; // moved onto a non-submenu item -> collapse

    if (new_hover != m_hover || new_hover_sub != m_hover_sub || new_open_sub != m_open_sub) {
        m_hover = new_hover;
        m_hover_sub = new_hover_sub;
        m_open_sub = new_open_sub;
        rebuild();
    }
    return true;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
