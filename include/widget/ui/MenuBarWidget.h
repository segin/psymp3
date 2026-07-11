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
    // One menu entry. A leaf has an action (and optionally a `checked` predicate
    // that draws a radio dot); a separator draws a divider; a submenu holds
    // child items and ignores `action`.
    struct Item {
        std::string label;
        std::function<void()> action;    // invoked on click for leaf items
        std::function<bool()> checked;    // optional; draws a radio dot when true
        std::vector<Item> submenu;        // non-empty => submenu
        bool separator = false;

        static Item leaf(std::string l, std::function<void()> a,
                         std::function<bool()> c = nullptr) {
            Item i; i.label = std::move(l); i.action = std::move(a); i.checked = std::move(c); return i;
        }
        static Item sep() { Item i; i.separator = true; return i; }
        static Item sub(std::string l, std::vector<Item> items) {
            Item i; i.label = std::move(l); i.submenu = std::move(items); return i;
        }
    };

    MenuBarWidget(int width, int height, Font* font);

    void addMenu(std::string name, std::vector<Item> items);

    bool isOpen() const { return m_open >= 0; }
    void closeMenu();

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;

    static constexpr int BAR_H = 16; // menu bar height (logical px)

private:
    struct Menu {
        std::string name;
        std::vector<Item> items;
        int bar_x = 0;   // computed on addMenu
        int bar_w = 0;
    };

    void rebuild();                 // repaint the overlay surface from state
    std::unique_ptr<Surface> renderText(const std::string& s, uint8_t r, uint8_t g, uint8_t b) const;
    int  textWidth(const std::string& s) const;   // cached pixel width
    mutable std::unordered_map<std::string, int> m_text_w;

    // Layout / hit-testing (shared by draw and event handling).
    int  barHitTest(int x, int y) const;                  // top-level index or -1
    Rect dropdownRect(const Menu& m) const;               // popup box
    int  itemTopY(const std::vector<Item>& items, int i) const; // y offset within a popup
    int  popupHeight(const std::vector<Item>& items) const;
    int  popupWidth(const std::vector<Item>& items) const;
    int  itemAt(const std::vector<Item>& items, const Rect& box, int x, int y) const; // index or -1
    Rect submenuRect(const Menu& m, int item_index) const;

    Font* m_font;               // non-owning
    std::vector<Menu> m_menus;
    int m_open = -1;            // open top-level menu, or -1
    int m_hover = -1;          // hovered item index in the open dropdown
    int m_open_sub = -1;       // submenu item index that is expanded, or -1
    int m_hover_sub = -1;      // hovered item index in the open submenu

    static constexpr int ITEM_H = 16;   // dropdown item row height
    static constexpr int SEP_H = 6;     // separator row height
    static constexpr int CHECK_COL = 16; // left column for the radio dot
    static constexpr int ARROW_COL = 14; // right column for the submenu arrow
    static constexpr int BAR_PAD = 7;    // horizontal padding per bar item
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // MENUBARWIDGET_H
