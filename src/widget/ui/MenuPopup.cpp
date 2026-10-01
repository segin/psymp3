/*
 * MenuPopup.cpp - The one popup-menu implementation, shared by every menu.
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

MenuPopup::MenuPopup(Font* font)
    : m_font(font)
{
}

// ---- Text ---------------------------------------------------------------

std::unique_ptr<Surface> MenuPopup::renderText(const std::string& s, SDL_Color fg, SDL_Color bg) const
{
    if (!m_font || s.empty()) return nullptr;
    return m_font->RenderLCD(TagLib::String(s, TagLib::String::UTF8),
                             fg.r, fg.g, fg.b, bg.r, bg.g, bg.b);
}

int MenuPopup::textWidth(const std::string& s) const
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

std::string MenuPopup::parseMnemonic(const std::string& label, int* off, int* w) const
{
    if (off) *off = -1;
    if (w) *w = 0;

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
        if (off) *off = textWidth(clean.substr(0, mn_index));
        if (w) *w = textWidth(clean.substr(mn_index, end - mn_index));
    }
    return clean;
}

int MenuPopup::mnemonicChar(const std::string& label)
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

void MenuPopup::drawLabel(Surface& surface, const std::string& label, int x, int row_y,
                          int row_h, SDL_Color fg, SDL_Color bg) const
{
    int mn_off = -1, mn_w = 0;
    std::string clean = parseMnemonic(label, &mn_off, &mn_w);
    auto t = renderText(clean, fg, bg);
    if (!t) return;
    int ty = row_y + (row_h - t->height()) / 2;
    surface.Blit(*t, Rect(x, ty, t->width(), t->height()));
    if (mn_off >= 0 && mn_w > 0) {
        int uy = ty + t->height() - 2; // just under the glyph baseline
        surface.hline(x + mn_off, x + mn_off + mn_w - 1, uy, fg.r, fg.g, fg.b, 255);
    }
}

// ---- Layout -------------------------------------------------------------

int MenuPopup::itemTopY(const std::vector<MenuItem>& items, int i) const
{
    int y = 1; // top border
    for (int k = 0; k < i && k < static_cast<int>(items.size()); ++k)
        y += items[k].separator ? SEP_H : ITEM_H;
    return y;
}

int MenuPopup::popupHeight(const std::vector<MenuItem>& items) const
{
    int h = 2; // top+bottom border
    for (const auto& it : items) h += it.separator ? SEP_H : ITEM_H;
    return h;
}

int MenuPopup::popupWidth(const std::vector<MenuItem>& items) const
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

int MenuPopup::itemAt(const std::vector<MenuItem>& items, const Rect& box, int x, int y) const
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

void MenuPopup::open(std::vector<MenuItem> items, int x, int y, const Rect& bounds)
{
    m_items = std::move(items);
    m_bounds = bounds;
    const int w = popupWidth(m_items);
    const int h = popupHeight(m_items);
    const int right = bounds.x() + bounds.width();
    const int bottom = bounds.y() + bounds.height();
    if (x + w > right) x = right - w; // keep it within bounds
    if (y + h > bottom) y = bottom - h;
    if (x < bounds.x()) x = bounds.x();
    if (y < bounds.y()) y = bounds.y();
    m_box = Rect(x, y, w, h);
    m_hover = -1;
    m_open_sub = -1;
    m_hover_sub = -1;
    m_open = true;
}

void MenuPopup::close()
{
    m_open = false;
    m_items.clear();
    m_hover = -1;
    m_open_sub = -1;
    m_hover_sub = -1;
}

void MenuPopup::selectFirst()
{
    m_hover = firstSelectable(m_items);
    m_open_sub = -1;
    m_hover_sub = -1;
}

Rect MenuPopup::submenuBox() const
{
    const auto& sub = m_items[m_open_sub].submenu;
    int w = popupWidth(sub);
    int h = popupHeight(sub);
    int x = m_box.x() + m_box.width() - 1;
    int y = m_box.y() + itemTopY(m_items, m_open_sub) - 1;
    const int right = m_bounds.x() + m_bounds.width();
    const int bottom = m_bounds.y() + m_bounds.height();
    if (x + w > right) x = m_box.x() - w + 1; // flip left if it doesn't fit
    if (x < m_bounds.x()) x = m_bounds.x();
    if (y + h > bottom) y = bottom - h;
    if (y < m_bounds.y()) y = m_bounds.y();
    return Rect(x, y, w, h);
}

bool MenuPopup::contains(int x, int y) const
{
    if (!m_open) return false;
    if (m_box.contains(x, y)) return true;
    if (m_open_sub >= 0 && !m_items[m_open_sub].submenu.empty() && submenuBox().contains(x, y)) return true;
    return false;
}

// Separators and disabled items are never keyboard-selected: a disabled item
// draws no highlight, so selecting one would make the selection vanish.
int MenuPopup::firstSelectable(const std::vector<MenuItem>& items)
{
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        if (!items[i].separator && items[i].isEnabled()) return i;
    return -1;
}

int MenuPopup::stepSelectable(const std::vector<MenuItem>& items, int from, int dir)
{
    int n = static_cast<int>(items.size());
    if (n == 0) return -1;
    if (from < 0) {
        // No current selection: enter the list from the edge the step
        // direction implies, so Up selects the LAST item.
        from = (dir < 0) ? n : -1;
    }
    for (int k = 0; k < n; ++k) {
        from = (from + dir + n) % n;
        if (!items[from].separator && items[from].isEnabled()) return from;
    }
    return -1;
}

// ---- Drawing ------------------------------------------------------------

void MenuPopup::drawBox(Surface& surface, const std::vector<MenuItem>& items, const Rect& box, int hover) const
{
    surface.box(box.x(), box.y(), box.x() + box.width() - 1, box.y() + box.height() - 1,
                kFace.r, kFace.g, kFace.b, 255);
    // Raised border: light top/left, shadow bottom/right.
    const int x1 = box.x(), y1 = box.y(), x2 = box.x() + box.width() - 1, y2 = box.y() + box.height() - 1;
    surface.hline(x1, x2, y1, kLight.r, kLight.g, kLight.b, 255);
    surface.vline(x1, y1, y2, kLight.r, kLight.g, kLight.b, 255);
    surface.hline(x1, x2, y2, kShadow.r, kShadow.g, kShadow.b, 255);
    surface.vline(x2, y1, y2, kShadow.r, kShadow.g, kShadow.b, 255);

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const MenuItem& it = items[i];
        int ry = box.y() + itemTopY(items, i);
        if (it.separator) {
            surface.hline(box.x() + 2, box.x() + box.width() - 3, ry + SEP_H / 2,
                          kShadow.r, kShadow.g, kShadow.b, 255);
            continue;
        }
        // Disabled items are greyed and never show the hover highlight.
        bool en = it.isEnabled();
        bool hi = (i == hover) && en;
        if (hi)
            surface.box(box.x() + 1, ry, box.x() + box.width() - 2, ry + ITEM_H - 1,
                        kHiBg.r, kHiBg.g, kHiBg.b, 255);
        SDL_Color tc = !en ? kDisabled : (hi ? kHiText : kText);
        SDL_Color bg = hi ? kHiBg : kFace;
        // radio dot
        if (it.checked && it.checked()) {
            int cx = box.x() + 5, cyd = ry + ITEM_H / 2 - 2;
            surface.box(cx, cyd, cx + 3, cyd + 3, tc.r, tc.g, tc.b, 255);
        }
        drawLabel(surface, it.label, box.x() + CHECK_COL, ry, ITEM_H, tc, bg);
        // right-aligned keyboard-shortcut hint
        if (!it.shortcut.empty()) {
            if (auto st = renderText(it.shortcut, tc, bg)) {
                int sx = box.x() + box.width() - ARROW_COL - st->width() - 2;
                int sy = ry + (ITEM_H - st->height()) / 2;
                surface.Blit(*st, Rect(sx, sy, st->width(), st->height()));
            }
        }
        // submenu arrow
        if (!it.submenu.empty()) {
            int ax = box.x() + box.width() - ARROW_COL + 3;
            int ay = ry + ITEM_H / 2;
            for (int k = 0; k < 4; ++k)
                surface.vline(ax + k, ay - (3 - k), ay + (3 - k), tc.r, tc.g, tc.b, 255);
        }
    }
}

void MenuPopup::draw(Surface& surface) const
{
    if (!m_open) return;
    drawBox(surface, m_items, m_box, m_hover);
    if (m_open_sub >= 0 && m_open_sub < static_cast<int>(m_items.size())
        && !m_items[m_open_sub].submenu.empty()) {
        drawBox(surface, m_items[m_open_sub].submenu, submenuBox(), m_hover_sub);
    }
}

// ---- Mouse --------------------------------------------------------------

bool MenuPopup::mouseDown(int x, int y)
{
    if (!m_open) return false;
    // Items are only HIGHLIGHTED on press; they are picked on release, so a
    // press can slide away to cancel and a press-drag-release gesture picks
    // in one motion. The submenu first (it overlaps to the right).
    if (m_open_sub >= 0 && !m_items[m_open_sub].submenu.empty()) {
        int si = itemAt(m_items[m_open_sub].submenu, submenuBox(), x, y);
        if (si >= 0) {
            if (m_hover_sub == si) return false;
            m_hover_sub = si;
            return true;
        }
    }
    int di = itemAt(m_items, m_box, x, y);
    if (di < 0) return false;
    if (!m_items[di].submenu.empty()) {
        m_open_sub = (m_open_sub == di) ? -1 : di;
        m_hover = di;
        m_hover_sub = -1;
        return true;
    }
    if (m_hover == di) return false;
    m_hover = di;
    return true;
}

bool MenuPopup::mouseUp(int x, int y, std::function<void()>& picked)
{
    picked = nullptr;
    if (!m_open) return false;
    // Submenu first (it overlaps the menu).
    if (m_open_sub >= 0 && !m_items[m_open_sub].submenu.empty()) {
        int si = itemAt(m_items[m_open_sub].submenu, submenuBox(), x, y);
        if (si >= 0) {
            const MenuItem& sit = m_items[m_open_sub].submenu[si];
            if (sit.isEnabled()) picked = sit.action ? sit.action : [] {};
            return false;
        }
    }
    int di = itemAt(m_items, m_box, x, y);
    if (di >= 0 && m_items[di].submenu.empty() && m_items[di].isEnabled()) {
        picked = m_items[di].action ? m_items[di].action : [] {};
    }
    return false;
}

bool MenuPopup::mouseMotion(int x, int y)
{
    if (!m_open) return false;
    int new_hover = itemAt(m_items, m_box, x, y);
    int new_hover_sub = -1;
    if (m_open_sub >= 0 && !m_items[m_open_sub].submenu.empty())
        new_hover_sub = itemAt(m_items[m_open_sub].submenu, submenuBox(), x, y);

    // Hovering a submenu-parent item expands it; moving onto another item
    // collapses it.
    int new_open_sub = m_open_sub;
    if (new_hover >= 0 && !m_items[new_hover].submenu.empty())
        new_open_sub = new_hover;
    else if (new_hover >= 0 && new_hover_sub < 0)
        new_open_sub = -1;

    if (new_hover != m_hover || new_hover_sub != m_hover_sub || new_open_sub != m_open_sub) {
        m_hover = new_hover;
        m_hover_sub = new_hover_sub;
        m_open_sub = new_open_sub;
        return true;
    }
    return false;
}

// ---- Keyboard -----------------------------------------------------------

MenuPopup::KeyResult MenuPopup::key(const SDL_keysym& keysym, std::function<void()>& picked)
{
    picked = nullptr;
    if (!m_open) return KeyResult::Handled;

    bool in_sub = (m_open_sub >= 0 && m_hover_sub >= 0
                   && !m_items[m_open_sub].submenu.empty());
    std::vector<MenuItem>& list = in_sub ? m_items[m_open_sub].submenu : m_items;
    int& sel = in_sub ? m_hover_sub : m_hover;

    auto pick = [&](MenuItem& it) {
        if (!it.isEnabled()) return KeyResult::Handled; // disabled: keep the menu open
        picked = it.action ? it.action : [] {};
        return KeyResult::Picked;
    };

    switch (keysym.sym) {
        case SDLK_ESCAPE:
            if (in_sub) { m_open_sub = -1; m_hover_sub = -1; return KeyResult::Handled; }
            return KeyResult::Close;

        case SDLK_DOWN:
            sel = stepSelectable(list, sel, +1);
            return KeyResult::Handled;

        case SDLK_UP:
            sel = stepSelectable(list, sel, -1);
            return KeyResult::Handled;

        case SDLK_RIGHT:
            // Enter a submenu if the selection has one; otherwise (also from
            // within a submenu) it's the owner's: the menu bar moves to the
            // next menu.
            if (!in_sub && m_hover >= 0 && !m_items[m_hover].submenu.empty()) {
                m_open_sub = m_hover;
                m_hover_sub = firstSelectable(m_items[m_hover].submenu);
                return KeyResult::Handled;
            }
            return KeyResult::Next;

        case SDLK_LEFT:
            if (in_sub) { m_open_sub = -1; m_hover_sub = -1; return KeyResult::Handled; }
            return KeyResult::Previous;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: {
            if (sel < 0 || sel >= static_cast<int>(list.size())) return KeyResult::Handled;
            MenuItem& it = list[sel];
            if (!it.submenu.empty()) {
                if (!in_sub) {
                    m_open_sub = sel;
                    m_hover_sub = firstSelectable(it.submenu);
                }
                return KeyResult::Handled;
            }
            return pick(it);
        }

        default: break;
    }

    // Bare mnemonic key: jump to (and pick / expand) the matching item. Any
    // printable character can be a mnemonic, as in Windows ("&1 file.mp3").
    // As in Windows too, when several enabled items share it, the key only
    // moves the selection to the next of them, so each stays reachable
    // ("&Restart Track" and "&Repeat"); a unique one acts at once.
    if (keysym.sym > ' ' && keysym.sym < 0x7F) {
        std::vector<int> matches;
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            if (list[i].separator || !list[i].isEnabled()) continue;
            if (mnemonicChar(list[i].label) != static_cast<int>(keysym.sym)) continue;
            matches.push_back(i);
        }
        if (matches.size() > 1) {
            int next = matches.front();
            for (int m : matches) {
                if (m > sel) { next = m; break; }
            }
            sel = next;
            return KeyResult::Handled;
        }
        if (matches.size() == 1) {
            const int i = matches.front();
            sel = i;
            MenuItem& it = list[i];
            if (!it.submenu.empty()) {
                if (!in_sub) {
                    m_open_sub = i;
                    m_hover_sub = firstSelectable(it.submenu);
                }
                return KeyResult::Handled;
            }
            return pick(it);
        }
    }
    return KeyResult::Handled; // everything is consumed while a menu is open
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
