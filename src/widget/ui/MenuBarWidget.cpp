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
    // Measure with the same (LCD) render path used for drawing so the measured
    // width matches; the background colour is irrelevant to the advance width.
    if (auto surf = renderText(s, kText, kFace)) {
        w = surf->width();
    }
    m_text_w[s] = w;
    return w;
}

std::unique_ptr<Surface> MenuBarWidget::renderText(const std::string& s, SDL_Color fg, SDL_Color bg) const
{
    if (!m_font || s.empty()) return nullptr;
    return m_font->RenderLCD(TagLib::String(s, TagLib::String::UTF8),
                             fg.r, fg.g, fg.b, bg.r, bg.g, bg.b);
}

std::string MenuBarWidget::parseMnemonic(const std::string& label, int* mn_off, int* mn_w) const
{
    if (mn_off) *mn_off = -1;
    if (mn_w) *mn_w = 0;

    std::string clean;
    clean.reserve(label.size());
    int mn_index = -1; // byte index of the mnemonic char within `clean`
    for (size_t i = 0; i < label.size(); ++i) {
        if (label[i] == '&') {
            if (i + 1 < label.size() && label[i + 1] == '&') { clean += '&'; ++i; continue; }
            if (i + 1 < label.size() && mn_index < 0) { mn_index = static_cast<int>(clean.size()); continue; }
            continue; // trailing '&' - ignore
        }
        clean += label[i];
    }

    if (mn_index >= 0) {
        // Span the whole UTF-8 codepoint at the mnemonic position.
        size_t end = static_cast<size_t>(mn_index) + 1;
        while (end < clean.size() && (static_cast<unsigned char>(clean[end]) & 0xC0) == 0x80) ++end;
        if (mn_off) *mn_off = textWidth(clean.substr(0, mn_index));
        if (mn_w) *mn_w = textWidth(clean.substr(mn_index, end - mn_index));
    }
    return clean;
}

void MenuBarWidget::drawLabel(Surface& surf, const std::string& label, int x, int row_y,
                              int row_h, SDL_Color fg, SDL_Color bg)
{
    int mn_off = -1, mn_w = 0;
    std::string clean = parseMnemonic(label, &mn_off, &mn_w);
    auto t = renderText(clean, fg, bg);
    if (!t) return;
    int ty = row_y + (row_h - t->height()) / 2;
    surf.Blit(*t, Rect(x, ty, t->width(), t->height()));
    if (mn_off >= 0 && mn_w > 0) {
        int uy = ty + t->height() - 2; // just under the glyph baseline
        surf.hline(x + mn_off, x + mn_off + mn_w - 1, uy, fg.r, fg.g, fg.b, 255);
    }
}

void MenuBarWidget::addMenu(std::string name, std::vector<Item> items)
{
    Menu m;
    m.name = std::move(name);
    m.items = std::move(items);
    int x = 0;
    for (const auto& e : m_menus) x = e.bar_x + e.bar_w;
    m.bar_x = x;
    m.bar_w = textWidth(parseMnemonic(m.name, nullptr, nullptr)) + BAR_PAD * 2;
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
    int maxw = 0, maxsc = 0;
    for (const auto& it : items) {
        if (it.separator) continue;
        maxw = std::max(maxw, textWidth(parseMnemonic(it.label, nullptr, nullptr)));
        if (!it.shortcut.empty()) maxsc = std::max(maxsc, textWidth(it.shortcut));
    }
    int sc_col = maxsc > 0 ? maxsc + SHORTCUT_PAD : 0;
    return CHECK_COL + maxw + sc_col + ARROW_COL + 4;
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
        SDL_Color bg = open ? kHiBg : kBar;
        drawLabel(*surf, m.name, m.bar_x + BAR_PAD, 0, BAR_H, tc, bg);
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
            SDL_Color bg = hi ? kHiBg : kFace;
            // radio dot
            if (it.checked && it.checked()) {
                int cx = box.x() + 5, cyd = ry + ITEM_H / 2 - 2;
                surf->box(cx, cyd, cx + 3, cyd + 3, tc.r, tc.g, tc.b, 255);
            }
            drawLabel(*surf, it.label, box.x() + CHECK_COL, ry, ITEM_H, tc, bg);
            // right-aligned keyboard-shortcut hint
            if (!it.shortcut.empty()) {
                if (auto st = renderText(it.shortcut, tc, bg)) {
                    int sx = box.x() + box.width() - ARROW_COL - st->width() - 2;
                    int sy = ry + (ITEM_H - st->height()) / 2;
                    surf->Blit(*st, Rect(sx, sy, st->width(), st->height()));
                }
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

// ---- Keyboard -----------------------------------------------------------

int MenuBarWidget::mnemonicChar(const std::string& label)
{
    for (size_t i = 0; i + 1 < label.size(); ++i) {
        if (label[i] == '&') {
            if (label[i + 1] == '&') { ++i; continue; }   // literal "&&"
            unsigned char c = static_cast<unsigned char>(label[i + 1]);
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            return c;
        }
    }
    return 0;
}

int MenuBarWidget::firstSelectable(const std::vector<Item>& items)
{
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        if (!items[i].separator) return i;
    return -1;
}

int MenuBarWidget::stepSelectable(const std::vector<Item>& items, int from, int dir)
{
    int n = static_cast<int>(items.size());
    if (n == 0) return -1;
    for (int k = 0; k < n; ++k) {
        from = (from + dir % n + n) % n;
        if (!items[from].separator) return from;
    }
    return -1;
}

void MenuBarWidget::openMenu(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(m_menus.size())) return;
    m_open = idx;
    m_hover = firstSelectable(m_menus[idx].items);
    m_open_sub = -1;
    m_hover_sub = -1;
    rebuild();
}

bool MenuBarWidget::handleKey(const SDL_keysym& keysym)
{
    // --- closed: Alt+<mnemonic> opens the matching menu ---
    if (m_open < 0) {
        if ((keysym.mod & (KMOD_LALT | KMOD_RALT))
            && keysym.sym >= 'a' && keysym.sym <= 'z') {
            for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
                if (mnemonicChar(m_menus[i].name) == static_cast<int>(keysym.sym)) {
                    openMenu(i);
                    return true;
                }
            }
        }
        return false;
    }

    // --- open: the menu is modal for the keyboard ---
    Menu& m = m_menus[m_open];
    bool in_sub = (m_open_sub >= 0 && m_hover_sub >= 0
                   && !m.items[m_open_sub].submenu.empty());
    std::vector<Item>& list = in_sub ? m.items[m_open_sub].submenu : m.items;
    int& sel = in_sub ? m_hover_sub : m_hover;

    switch (keysym.sym) {
        case SDLK_ESCAPE:
            if (in_sub) { m_open_sub = -1; m_hover_sub = -1; rebuild(); }
            else closeMenu();
            return true;

        case SDLK_DOWN:
            sel = stepSelectable(list, sel, +1);
            rebuild();
            return true;

        case SDLK_UP:
            sel = stepSelectable(list, sel, -1);
            rebuild();
            return true;

        case SDLK_RIGHT:
            // Enter a submenu if the selection has one; otherwise move to the
            // next top-level menu.
            if (!in_sub && m_hover >= 0 && !m.items[m_hover].submenu.empty()) {
                m_open_sub = m_hover;
                m_hover_sub = firstSelectable(m.items[m_hover].submenu);
                rebuild();
            } else {
                openMenu((m_open + 1) % static_cast<int>(m_menus.size()));
            }
            return true;

        case SDLK_LEFT:
            // Leave a submenu, or move to the previous top-level menu.
            if (in_sub) { m_open_sub = -1; m_hover_sub = -1; rebuild(); }
            else {
                int n = static_cast<int>(m_menus.size());
                openMenu((m_open - 1 + n) % n);
            }
            return true;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: {
            if (sel < 0 || sel >= static_cast<int>(list.size())) return true;
            Item& it = list[sel];
            if (!it.submenu.empty()) {
                if (!in_sub) {
                    m_open_sub = sel;
                    m_hover_sub = firstSelectable(it.submenu);
                    rebuild();
                }
                return true;
            }
            auto act = it.action;
            closeMenu();
            if (act) act();
            return true;
        }

        default: break;
    }

    // Bare mnemonic letter: jump to (and activate/expand) the matching item.
    if (keysym.sym >= 'a' && keysym.sym <= 'z') {
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            if (list[i].separator) continue;
            if (mnemonicChar(list[i].label) != static_cast<int>(keysym.sym)) continue;
            sel = i;
            Item& it = list[i];
            if (!it.submenu.empty()) {
                if (!in_sub) {
                    m_open_sub = i;
                    m_hover_sub = firstSelectable(it.submenu);
                }
                rebuild();
            } else {
                auto act = it.action;
                closeMenu();
                if (act) act();
            }
            return true;
        }
    }

    return true; // consume everything while a menu is open
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
