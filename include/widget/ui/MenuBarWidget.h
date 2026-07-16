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
    // `label` may carry a Win32-style '&' mnemonic marker: the character after
    // '&' is drawn underlined (an accelerator hint); "&&" is a literal '&'.
    // `shortcut` is a right-aligned key hint (e.g. "Z") — display only; the
    // actual key is handled globally by the player.
    struct Item {
        std::string label;
        std::function<void()> action;    // invoked on click for leaf items
        std::function<bool()> checked;    // optional; draws a radio dot when true
        std::function<bool()> enabled;    // optional; false => greyed, not clickable
        std::vector<Item> submenu;        // non-empty => submenu
        std::string shortcut;             // right-aligned accelerator hint
        bool separator = false;

        static Item leaf(std::string l, std::function<void()> a,
                         std::function<bool()> c = nullptr, std::string sc = "",
                         std::function<bool()> en = nullptr) {
            Item i; i.label = std::move(l); i.action = std::move(a);
            i.checked = std::move(c); i.shortcut = std::move(sc);
            i.enabled = std::move(en); return i;
        }
        static Item sep() { Item i; i.separator = true; return i; }
        static Item sub(std::string l, std::vector<Item> items) {
            Item i; i.label = std::move(l); i.submenu = std::move(items); return i;
        }
        // A leaf with no enabled predicate is always enabled.
        bool isEnabled() const { return !enabled || enabled(); }
    };

    MenuBarWidget(int width, int height, Font* font);

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
    // ClearType (LCD) text, pre-blended against bg; the same bg must be painted
    // underneath before the returned (opaque) surface is blitted.
    std::unique_ptr<Surface> renderText(const std::string& s, SDL_Color fg, SDL_Color bg) const;
    int  textWidth(const std::string& s) const;   // cached pixel width
    mutable std::unordered_map<std::string, int> m_text_w;

    // Strip the '&' mnemonic marker, returning the display string. Sets *mn_off
    // to the pixel x-offset (within the rendered string) of the underlined
    // glyph and *mn_w to its width, or *mn_off = -1 when there is no mnemonic.
    std::string parseMnemonic(const std::string& label, int* mn_off, int* mn_w) const;
    // Lowercased mnemonic character of a label (char after a single '&'), or 0.
    static int mnemonicChar(const std::string& label);
    // First non-separator item index, or -1 if none.
    static int firstSelectable(const std::vector<Item>& items);
    // Next non-separator item from `from` stepping by `dir` (+1/-1), wrapping.
    static int stepSelectable(const std::vector<Item>& items, int from, int dir);
    void openMenu(int idx);   // open top-level menu `idx`, select its first item
    // Draw a label (with mnemonic underline) vertically centred in a row.
    void drawLabel(Surface& surf, const std::string& label, int x, int row_y,
                   int row_h, SDL_Color fg, SDL_Color bg);

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
    int m_rows = 1;            // number of bar rows after wrapping
    int m_open = -1;            // open top-level menu, or -1
    int m_hover = -1;          // hovered item index in the open dropdown
    int m_open_sub = -1;       // submenu item index that is expanded, or -1
    int m_hover_sub = -1;      // hovered item index in the open submenu

    static constexpr int ITEM_H = 16;   // dropdown item row height
    static constexpr int SEP_H = 6;     // separator row height
    static constexpr int CHECK_COL = 16; // left column for the radio dot
    static constexpr int ARROW_COL = 14; // right column for the submenu arrow
    static constexpr int BAR_PAD = 7;    // horizontal padding per bar item
    static constexpr int SHORTCUT_PAD = 16; // gap between label and shortcut col
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // MENUBARWIDGET_H
