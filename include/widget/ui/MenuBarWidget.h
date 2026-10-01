/*
 * MenuBarWidget.h - In-app (SDL-drawn) menu bar with dropdowns and submenus.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MENUBARWIDGET_H
#define MENUBARWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// A software-drawn menu bar rendered as a full-window top-most overlay. Because
// it is painted and hit-tested by the normal widget/event loop (not a native
// OS menu), the rest of the UI keeps animating while a menu is open.
//
// The widget spans the whole logical surface; only the top bar and any open
// dropdown/submenu are opaque, the rest is transparent and passes clicks
// through to the widgets beneath.
class MenuBarWidget : public Widget
{
public:
    // The items are the same as every other menu's (see MenuPopup.h), and the
    // open drop-down is a MenuPopup.
    using Item = MenuItem;

    MenuBarWidget(int width, int height, Font* font);
    ~MenuBarWidget() override;

    // The open menu is drawn by the Player over everything, at screen
    // coordinates, so an embedded bar's menu (the equalizer's, the Playlist
    // Manager's) is not clipped by its window. While a menu is open, the
    // Player hands its bar every mouse event first, in screen coordinates
    // (true when consumed — always, while open): the bar's titles switch or
    // close menus, the menu highlights and picks, and a press anywhere else
    // closes it and is consumed.
    static void blitOpenMenu(Surface& target);
    static bool routeMouseDown(const SDL_MouseButtonEvent& event, int x, int y);
    static bool routeMouseMotion(const SDL_MouseMotionEvent& event, int x, int y);
    static bool routeMouseUp(const SDL_MouseButtonEvent& event, int x, int y);
    // Likewise the keyboard: the bar with a menu open, whichever window it
    // belongs to, takes every key (true when consumed — always, while open).
    static bool routeKey(const SDL_keysym& keysym);
    // Whether any bar has a menu open.
    static bool anyOpen() { return s_open_bar && s_open_bar->m_open >= 0; }

    void addMenu(std::string name, std::vector<Item> items);

    // Resize the overlay: re-flows the bar for the new width (wrapping titles to
    // more rows if they don't fit) and repaints at the new surface size.
    void resize(int width, int height);

    // Total height of the (possibly multi-row) bar, in px. Callers that place
    // content below the bar should offset by this, not by BAR_H.
    int barHeight() const { return m_rows * BAR_H; }

    bool isOpen() const { return m_open >= 0; }
    void closeMenu();

    // Keyboard driver. When no menu is open, Alt+<mnemonic> opens the matching
    // top-level menu (returns true if it did). When a menu is open, arrows move
    // the selection, Left/Right switch menus / enter-exit submenus, Enter/Space
    // activate, Esc backs out, and a bare mnemonic letter jumps to/activates an
    // item; every key is consumed while open so it cannot leak to global
    // shortcuts. Returns true when the key was handled.
    bool handleKey(const SDL_keysym& keysym);

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    // Leaf items activate on RELEASE (classic menu protocol): press on a bar
    // title, drag into the dropdown, release on an item selects it in one
    // gesture, and a press on an item can slide away to cancel.
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

    static constexpr int BAR_H = 20; // menu bar height (logical px; text + 2px top/bottom)

private:
    struct Menu {
        std::string name;
        std::vector<Item> items;
        int bar_x = 0;   // computed by layoutBar()
        int bar_y = 0;   // top of this menu's row
        int bar_w = 0;
    };

    void layoutBar();               // (re)flow the bar titles into rows for the width
    void rebuild();                 // repaint the overlay surface from state
    // Open top-level menu `idx` below its title; from the keyboard, with its
    // first item highlighted.
    void openMenu(int idx, bool select_first);
    int  barHitTest(int x, int y) const; // top-level index or -1 (local coordinates)
    Rect screenRect() const;            // where the bar is on the screen

    Font* m_font;               // non-owning
    std::vector<Menu> m_menus;
    int m_rows = 1;             // number of bar rows after wrapping
    int m_open = -1;            // open top-level menu, or -1
    MenuPopup m_popup;          // its drop-down, in screen coordinates

    static MenuBarWidget* s_open_bar; // the bar with a menu open, if any

    static constexpr int BAR_PAD = 7;    // horizontal padding per bar item
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // MENUBARWIDGET_H
