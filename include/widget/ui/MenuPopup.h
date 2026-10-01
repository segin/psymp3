/*
 * MenuPopup.h - The one popup-menu implementation: items, layout, drawing
 *               and interaction, shared by every menu in PsyMP3.
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef MENUPOPUP_H
#define MENUPOPUP_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// One menu entry, in every menu (the menu bar's drop-downs, context menus, the
// window control menu). A leaf has an action (and optionally a `checked`
// predicate that draws a radio dot); a separator draws a divider; a submenu
// holds child items and ignores `action`.
// `label` may carry a Win32-style '&' mnemonic marker: the character after
// '&' is drawn underlined, and pressing it while the menu is open picks the
// item; "&&" is a literal '&'. `shortcut` is a right-aligned key hint (e.g.
// "Ctrl+F4") — display only; the key itself is handled elsewhere.
struct MenuItem {
    std::string label;
    std::function<void()> action;    // invoked when a leaf item is picked
    std::function<bool()> checked;   // optional; draws a radio dot when true
    std::function<bool()> enabled;   // optional; false => greyed, not pickable
    std::vector<MenuItem> submenu;   // non-empty => submenu
    std::string shortcut;            // right-aligned accelerator hint
    bool separator = false;

    static MenuItem leaf(std::string l, std::function<void()> a,
                         std::function<bool()> c = nullptr, std::string sc = "",
                         std::function<bool()> en = nullptr) {
        MenuItem i; i.label = std::move(l); i.action = std::move(a);
        i.checked = std::move(c); i.shortcut = std::move(sc);
        i.enabled = std::move(en); return i;
    }
    // A leaf whose enabled state is fixed when the menu is built (the usual
    // case for a context menu, which is rebuilt each time it opens).
    static MenuItem command(std::string l, std::function<void()> a,
                            bool is_enabled = true, std::string sc = "") {
        MenuItem i; i.label = std::move(l); i.action = std::move(a);
        i.shortcut = std::move(sc);
        if (!is_enabled) i.enabled = [] { return false; };
        return i;
    }
    static MenuItem sep() { MenuItem i; i.separator = true; return i; }
    static MenuItem sub(std::string l, std::vector<MenuItem> items) {
        MenuItem i; i.label = std::move(l); i.submenu = std::move(items); return i;
    }
    // A leaf with no enabled predicate is always enabled.
    bool isEnabled() const { return !enabled || enabled(); }
};

// An open popup menu — a list of items and at most one expanded submenu —
// with its layout, drawing and mouse/keyboard handling. Coordinates are
// whatever space the owner opens it in; the owner draws it into a surface in
// that space and feeds it events there. The menu bar hosts one for its open
// drop-down; ContextMenuWidget hosts the one screen-wide context menu.
class MenuPopup {
public:
    // What a key did: consumed with nothing more to do; asked to close
    // (Escape at the top level); asked for the previous / next menu (Left /
    // Right at the top level, for the menu bar); or picked an item, whose
    // action is returned for the owner to run once it has closed the menu.
    enum class KeyResult { Handled, Close, Previous, Next, Picked };

    explicit MenuPopup(Font* font);

    // Show `items` with the box's top-left at (x, y), moved to stay within
    // `bounds` (submenus flip left to stay within it too). Nothing is
    // highlighted until the pointer or a key picks something.
    void open(std::vector<MenuItem> items, int x, int y, const Rect& bounds);
    void close();
    bool isOpen() const { return m_open; }
    // Highlight the first item (a menu opened from the keyboard).
    void selectFirst();

    Rect box() const { return m_box; }
    // True if (x, y) is on the menu or its open submenu.
    bool contains(int x, int y) const;

    // Mouse. Each returns whether the menu's appearance changed.
    // Motion highlights the item under the pointer and expands a submenu
    // under it. A press does the same (items are picked on release, so a
    // press can slide away to cancel). A release on an enabled leaf picks it:
    // `picked` receives its action (empty otherwise).
    bool mouseMotion(int x, int y);
    bool mouseDown(int x, int y);
    bool mouseUp(int x, int y, std::function<void()>& picked);

    // Keyboard: arrows move, Right/Left enter/leave a submenu, Enter/Space
    // pick, Escape backs out, and an item's mnemonic letter picks it.
    KeyResult key(const SDL_keysym& keysym, std::function<void()>& picked);

    void draw(Surface& surface) const;

    // The menu's palette and metrics (the menu bar draws its bar with them).
    static constexpr SDL_Color kBar     = {192, 192, 192, 255};
    static constexpr SDL_Color kFace    = {198, 198, 198, 255};
    static constexpr SDL_Color kLight   = {255, 255, 255, 255};
    static constexpr SDL_Color kShadow  = {110, 110, 110, 255};
    static constexpr SDL_Color kText    = {0, 0, 0, 255};
    static constexpr SDL_Color kHiBg    = {0, 0, 128, 255};
    static constexpr SDL_Color kHiText  = {255, 255, 255, 255};
    static constexpr SDL_Color kDisabled = {128, 128, 128, 255};
    static constexpr int ITEM_H = 16;       // item row height
    static constexpr int SEP_H = 6;         // separator row height
    static constexpr int CHECK_COL = 16;    // left column for the radio dot
    static constexpr int ARROW_COL = 14;    // right column for the submenu arrow
    static constexpr int SHORTCUT_PAD = 16; // gap between label and shortcut

    // Mnemonic helpers, for the menu bar's titles as well as the items.
    // The label without its '&' marker; *off / *w (if given) receive the
    // underlined glyph's pixel offset and width, *off = -1 when there is none.
    std::string parseMnemonic(const std::string& label, int* off, int* w) const;
    // Lowercased mnemonic character of a label, or 0.
    static int mnemonicChar(const std::string& label);
    // Draw a label, mnemonic underlined, vertically centred in a row.
    void drawLabel(Surface& surface, const std::string& label, int x, int row_y,
                   int row_h, SDL_Color fg, SDL_Color bg) const;
    std::unique_ptr<Surface> renderText(const std::string& s, SDL_Color fg, SDL_Color bg) const;
    int textWidth(const std::string& s) const; // cached

private:
    static int firstSelectable(const std::vector<MenuItem>& items);
    static int stepSelectable(const std::vector<MenuItem>& items, int from, int dir);
    int itemTopY(const std::vector<MenuItem>& items, int i) const;
    int popupWidth(const std::vector<MenuItem>& items) const;
    int popupHeight(const std::vector<MenuItem>& items) const;
    int itemAt(const std::vector<MenuItem>& items, const Rect& box, int x, int y) const;
    Rect submenuBox() const;   // the expanded submenu's box (valid when m_open_sub >= 0)
    void drawBox(Surface& surface, const std::vector<MenuItem>& items, const Rect& box, int hover) const;

    Font* m_font;
    std::vector<MenuItem> m_items;
    Rect m_box;
    Rect m_bounds;
    bool m_open = false;
    int m_hover = -1;      // highlighted item
    int m_open_sub = -1;   // item whose submenu is expanded, or -1
    int m_hover_sub = -1;  // highlighted submenu item
    mutable std::unordered_map<std::string, int> m_text_w;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // MENUPOPUP_H
