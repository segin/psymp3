/*
 * ContextMenuWidget.h - Right-click (context) menus
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef CONTEXTMENUWIDGET_H
#define CONTEXTMENUWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

using PsyMP3::Widget::Foundation::Widget;
using PsyMP3::Widget::Foundation::DrawableWidget;

/**
 * @brief Context menus: a MenuPopup (the same menu as the menu bar's) shown
 *        over everything, one at a time.
 *
 * The open menu belongs to no window. The Player draws it on top of all else
 * (blitOpenMenu) and gives it mouse and keyboard events before anything else
 * (route*), in screen coordinates. A press on an item highlights it and its
 * release picks it; a press anywhere else closes the menu and is consumed,
 * except inside the opener's pass-through area (the window control menu's
 * icon, whose own toggle closes it). Escape closes it; the menu's keys are
 * the menu bar's.
 *
 * Code that opens one from a widget can use popUp() directly with screen
 * coordinates. A ContextMenuWidget is a handle for owners that keep one
 * around: it holds the entries and opens them at a point relative to itself,
 * and draws nothing and takes no clicks of its own.
 */
class ContextMenuWidget : public Widget {
public:
    using Entry = MenuItem;

    ContextMenuWidget(int width, int height, Core::Font* font);
    ~ContextMenuWidget() override;

    // Replace the menu's items (used by the next openAt).
    void setEntries(std::vector<MenuItem> entries);
    // Show the menu with its top-left near (x, y), relative to this widget.
    void openAt(int x, int y);
    void close();
    bool isOpen() const;
    // Fired whenever the menu closes, by any path (item, dismissal, close()).
    // Owners use it to sync state that mirrors the open menu (e.g. the window
    // frame's inverted titlebar icon).
    void setOnClose(std::function<void()> cb) { m_on_close = std::move(cb); }
    // Presses inside `area` (relative to this widget) go to the widgets
    // beneath while the menu stays open.
    void setPassThrough(const Rect& area) { m_pass_through = area; }
    // Size of the handle (it covers its container; nothing is drawn).
    void resize(int width, int height);

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

    // Open `items` with the menu's top-left near (screen_x, screen_y), kept
    // within `screen_bounds`. `owner` identifies the opener for isOpenFor /
    // closeFor (and is dropped silently if it goes away while open); on_close
    // runs when the menu closes; presses in `pass_through` (screen) go to the
    // widgets beneath.
    static void popUp(Core::Font* font, std::vector<MenuItem> items, int screen_x, int screen_y,
                      const Rect& screen_bounds, const void* owner = nullptr,
                      std::function<void()> on_close = nullptr, const Rect& pass_through = Rect());
    static bool isOpenFor(const void* owner);
    // Whether any context menu is open, whoever owns it.
    static bool anyOpen();
    // Whether the open menu (submenu included) covers the screen point.
    static bool openMenuContains(int x, int y);
    static void closeFor(const void* owner);
    // The owner is being destroyed: forget the menu without running on_close.
    static void forget(const void* owner);

    // The Player's hooks: events in screen coordinates, true when consumed.
    static bool routeMouseDown(const SDL_MouseButtonEvent& event, int x, int y);
    static bool routeMouseMotion(const SDL_MouseMotionEvent& event, int x, int y);
    static bool routeMouseUp(const SDL_MouseButtonEvent& event, int x, int y);
    static bool routeKey(const SDL_keysym& keysym);
    static void blitOpenMenu(Surface& target);
    // The screen the menus are kept within; the Player sets it each frame.
    static void setScreenSize(int width, int height);
    static Rect screenBounds();
    // Where a widget is on the screen: its position plus its ancestors'.
    static Rect screenRectOf(const Widget* widget);

private:
    static void closeOpenMenu(); // runs the on_close of the menu that was open

    Core::Font* m_font;
    std::vector<MenuItem> m_entries;
    std::function<void()> m_on_close;
    Rect m_pass_through;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // CONTEXTMENUWIDGET_H
