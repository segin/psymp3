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

MenuBarWidget* MenuBarWidget::s_open_bar = nullptr;

MenuBarWidget::MenuBarWidget(int width, int height, Font* font)
    : m_font(font)
    , m_popup(font)
{
    setPos(Rect(0, 0, width, height));
    rebuild();
}

MenuBarWidget::~MenuBarWidget()
{
    if (s_open_bar == this) {
        s_open_bar = nullptr;
    }
}

Rect MenuBarWidget::screenRect() const
{
    return ContextMenuWidget::screenRectOf(this);
}

void MenuBarWidget::blitOpenMenu(Surface& target)
{
    if (s_open_bar && s_open_bar->m_open >= 0) {
        s_open_bar->m_popup.draw(target);
    }
}

bool MenuBarWidget::routeMouseDown(const SDL_MouseButtonEvent& event, int x, int y)
{
    MenuBarWidget* bar = s_open_bar;
    if (!bar || bar->m_open < 0) {
        return false;
    }
    const Rect me = bar->screenRect();
    const int title = bar->barHitTest(x - me.x(), y - me.y());
    if (event.button == SDL_BUTTON_LEFT && title >= 0) {
        // The open menu's title closes it; another title opens that menu.
        if (title == bar->m_open) {
            bar->closeMenu();
        } else {
            bar->openMenu(title, false);
        }
        return true;
    }
    if (event.button == SDL_BUTTON_LEFT && bar->m_popup.contains(x, y)) {
        // A press only highlights; the release picks.
        if (bar->m_popup.mouseDown(x, y)) bar->rebuild();
        return true;
    }
    // Anywhere else, or another button: close, and eat the click.
    bar->closeMenu();
    return true;
}

bool MenuBarWidget::routeMouseMotion(const SDL_MouseMotionEvent& event, int x, int y)
{
    (void)event;
    MenuBarWidget* bar = s_open_bar;
    if (!bar || bar->m_open < 0) {
        return false;
    }
    // Hovering another title while a menu is open switches to its menu.
    const Rect me = bar->screenRect();
    const int title = bar->barHitTest(x - me.x(), y - me.y());
    if (title >= 0 && title != bar->m_open) {
        bar->openMenu(title, false);
        return true;
    }
    if (bar->m_popup.mouseMotion(x, y)) bar->rebuild();
    return true;
}

bool MenuBarWidget::routeMouseUp(const SDL_MouseButtonEvent& event, int x, int y)
{
    MenuBarWidget* bar = s_open_bar;
    if (!bar || bar->m_open < 0) {
        return false;
    }
    // A release on an item picks it. A release anywhere else (a title, a
    // submenu parent, a separator, or off the menu after a drag) keeps the
    // menu open — this is what lets a simple click on a title open its menu.
    if (event.button == SDL_BUTTON_LEFT) {
        std::function<void()> picked;
        bar->m_popup.mouseUp(x, y, picked);
        if (picked) {
            bar->closeMenu();
            picked();
        }
    }
    return true;
}

void MenuBarWidget::layoutBar()
{
    // Flow the titles left-to-right; wrap to the next row when the next title
    // would exceed the widget width. Each row is BAR_H tall.
    const int W = getPos().width();
    int x = 0, row = 0;
    for (Menu& m : m_menus) {
        m.bar_w = m_popup.textWidth(m_popup.parseMnemonic(m.name, nullptr, nullptr)) + BAR_PAD * 2;
        if (x > 0 && x + m.bar_w > W) { // doesn't fit on this row -> wrap
            row++;
            x = 0;
        }
        m.bar_x = x;
        m.bar_y = row * BAR_H;
        x += m.bar_w;
    }
    m_rows = row + 1;
}

void MenuBarWidget::addMenu(std::string name, std::vector<Item> items)
{
    Menu m;
    m.name = std::move(name);
    m.items = std::move(items);
    m_menus.push_back(std::move(m));
    layoutBar();
    rebuild();
}

void MenuBarWidget::resize(int width, int height)
{
    setPos(Rect(0, 0, width, height));
    layoutBar();
    if (m_open >= 0) {
        openMenu(m_open, false); // re-place the drop-down for the new size
    }
    rebuild();
}

void MenuBarWidget::closeMenu()
{
    if (m_open >= 0) {
        m_open = -1;
        m_popup.close();
        if (s_open_bar == this) {
            s_open_bar = nullptr;
        }
        rebuild();
    }
}

void MenuBarWidget::openMenu(int idx, bool select_first)
{
    if (idx < 0 || idx >= static_cast<int>(m_menus.size())) return;
    // One bar's menu at a time.
    if (s_open_bar && s_open_bar != this) {
        s_open_bar->closeMenu();
    }
    s_open_bar = this;
    const Menu& m = m_menus[idx];
    m_open = idx;
    // Drop below this menu's title, on the screen (not clipped by a window),
    // kept within it.
    const Rect me = screenRect();
    m_popup.open(m.items, me.x() + m.bar_x, me.y() + m.bar_y + BAR_H,
                 ContextMenuWidget::screenBounds());
    if (select_first) {
        m_popup.selectFirst();
    }
    rebuild();
}

int MenuBarWidget::barHitTest(int x, int y) const
{
    if (y < 0 || y >= m_rows * BAR_H) return -1;
    for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
        const Menu& m = m_menus[i];
        if (x >= m.bar_x && x < m.bar_x + m.bar_w &&
            y >= m.bar_y && y < m.bar_y + BAR_H) return i;
    }
    return -1;
}

// ---- Drawing ------------------------------------------------------------

void MenuBarWidget::rebuild()
{
    const int W = getPos().width();
    const int H = getPos().height();
    auto surf = std::make_unique<Surface>(W, H, true);
    surf->FillRect(surf->MapRGBA(0, 0, 0, 0)); // transparent everywhere

    // --- the bar (one or more rows) ---
    const SDL_Color bar = MenuPopup::kBar;
    const SDL_Color shadow = MenuPopup::kShadow;
    const int bar_h = m_rows * BAR_H;
    surf->box(0, 0, W - 1, bar_h - 1, bar.r, bar.g, bar.b, 255);
    surf->hline(0, W - 1, bar_h - 1, shadow.r, shadow.g, shadow.b, 255);
    for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
        const Menu& m = m_menus[i];
        bool open = (i == m_open);
        const SDL_Color hi = MenuPopup::kHiBg;
        if (open)
            surf->box(m.bar_x, m.bar_y, m.bar_x + m.bar_w - 1, m.bar_y + BAR_H - 2,
                      hi.r, hi.g, hi.b, 255);
        SDL_Color tc = open ? MenuPopup::kHiText : MenuPopup::kText;
        SDL_Color bg = open ? MenuPopup::kHiBg : MenuPopup::kBar;
        m_popup.drawLabel(*surf, m.name, m.bar_x + BAR_PAD, m.bar_y, BAR_H, tc, bg);
    }
    // The open drop-down is drawn by blitOpenMenu, over everything.

    setSurface(std::move(surf));
    invalidate();
}

// ---- Events -------------------------------------------------------------

// While a menu is open the Player routes every mouse event to the open bar
// first (route*), so these see only a closed bar — but should one reach them
// anyway, they hand it on in screen coordinates.

bool MenuBarWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int x, int y)
{
    if (m_open >= 0) {
        const Rect me = screenRect();
        return routeMouseDown(event, me.x() + x, me.y() + y);
    }
    if (event.button != SDL_BUTTON_LEFT) {
        return false;
    }
    int bar = barHitTest(x, y);
    if (bar >= 0) {
        openMenu(bar, false);
        return true;
    }
    return false; // not on a title -> pass through
}

bool MenuBarWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int x, int y)
{
    if (m_open < 0) {
        return false;
    }
    const Rect me = screenRect();
    return routeMouseUp(event, me.x() + x, me.y() + y);
}

bool MenuBarWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int x, int y)
{
    if (m_open < 0) return false; // let hover pass through to widgets beneath
    const Rect me = screenRect();
    return routeMouseMotion(event, me.x() + x, me.y() + y);
}

// ---- Keyboard -----------------------------------------------------------

bool MenuBarWidget::handleKey(const SDL_keysym& keysym)
{
    // --- closed: Alt+<mnemonic> opens the matching menu ---
    if (m_open < 0) {
        if ((keysym.mod & (SDL_KMOD_LALT | SDL_KMOD_RALT))
            && keysym.sym > ' ' && keysym.sym < 0x7F) {
            for (int i = 0; i < static_cast<int>(m_menus.size()); ++i) {
                if (MenuPopup::mnemonicChar(m_menus[i].name) == static_cast<int>(keysym.sym)) {
                    openMenu(i, true);
                    return true;
                }
            }
        }
        return false;
    }

    // --- open: the menu is modal for the keyboard ---
    const int n = static_cast<int>(m_menus.size());
    std::function<void()> picked;
    switch (m_popup.key(keysym, picked)) {
        case MenuPopup::KeyResult::Handled:
            rebuild();
            break;
        case MenuPopup::KeyResult::Close:
            closeMenu();
            break;
        case MenuPopup::KeyResult::Previous:
            openMenu((m_open - 1 + n) % n, true);
            break;
        case MenuPopup::KeyResult::Next:
            openMenu((m_open + 1) % n, true);
            break;
        case MenuPopup::KeyResult::Picked:
            closeMenu();
            if (picked) picked();
            break;
    }
    return true; // every key is consumed while a menu is open
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
