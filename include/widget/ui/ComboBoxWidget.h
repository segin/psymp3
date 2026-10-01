/*
 * ComboBoxWidget.h - Windows 3.1 style drop-down list (combo box)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef COMBOBOXWIDGET_H
#define COMBOBOXWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// A drop-down list: a field showing the chosen item beside a button with the
// arrow-and-bar glyph. Pressing it opens a list of the items under it (above
// it when the screen has no room below), up to kMaxVisibleRows tall with a
// scrollbar beyond that. The item under the pointer is highlighted; releasing
// a press on one (a click, or a press on the field dragged onto it) picks it.
// Holding a press and dragging above or below the list scrolls it, the
// highlight going along. A click anywhere else closes the list and is
// consumed.
//
// The open list belongs to no window: it is drawn over everything by
// blitOpenList(), and the Player hands it mouse events before anything else
// through the routeOpenList*() functions, in screen coordinates. Only one
// list is open at a time.
//
// Keyboard, while focused: Up/Down, Home/End and PgUp/PgDn change the choice
// (also while open, where the highlight moves with it and wears the focus
// ants; a highlight that follows the mouse doesn't); Alt+Down or F4 opens;
// Enter, Alt+Up or F4 closes on the highlighted item; Escape closes; a letter
// jumps to the next item starting with it. While the list is open the field
// drops its highlight, as Windows 3.1 draws it.
class ComboBoxWidget : public Widget {
public:
    ComboBoxWidget(int width, int height, Font* font);
    ~ComboBoxWidget() override;

    void setItems(std::vector<TagLib::String> items);
    size_t itemCount() const { return m_items.size(); }
    int getSelectedIndex() const { return m_selected; }
    // Changes the choice without firing the change callback.
    void setSelectedIndex(int index);
    // Fired when the user picks a different item.
    void setOnChange(std::function<void(int)> cb) { m_on_change = std::move(cb); }
    bool isOpen() const { return s_open_widget == this; }

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    // Closed, the wheel steps the choice as Up/Down do (the open list's
    // wheel scrolls it, through routeOpenListMouseWheel).
    bool handleMouseWheel(int delta, int relative_x, int relative_y) override;
    // Records where the control lands on screen, which places the open list.
    void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos) override;
    void setEnabled(bool enabled) override;

    // Keyboard focus, following the other focusable widgets' pattern.
    static void clearFocusedWidget();
    static bool handleFocusedKeyPress(const SDL_keysym& keysym);
    static ComboBoxWidget* focusedWidget() { return s_focused_widget; }
    void takeFocus() { focus(); } // Tab-traversal entry point

    // The open list, if any: drawn over everything, and given mouse events
    // first (screen coordinates). Each returns true when it consumed the event.
    static void blitOpenList(Surface& target);
    static bool routeOpenListMouseDown(const SDL_MouseButtonEvent& event, int x, int y);
    static bool routeOpenListMouseMotion(const SDL_MouseMotionEvent& event, int x, int y);
    static bool routeOpenListMouseUp(const SDL_MouseButtonEvent& event, int x, int y);
    static bool routeOpenListMouseWheel(int delta);

private:
    static constexpr int kButtonWidth = 17;    // Windows 3.1's, divider to border
    static constexpr int kMaxVisibleRows = 8;
    static constexpr int kScrollbarWidth = 17;

    void focus();
    void blur();
    void open();
    // Close the list; with commit, the highlighted item becomes the choice.
    void close(bool commit);
    void choose(int index);            // set the choice and fire m_on_change
    void setHot(int index);            // highlight a list row, scrolling to it
    void setTop(int top);              // first visible list row, clamped

    void rebuildSurface();
    void rebuildList();
    int visibleRows() const;
    int itemHeight() const;
    // The open list's rectangle relative to the control's top-left corner.
    Rect listRect() const;
    bool hasScrollbar() const;
    int listRowAt(int relative_x, int relative_y) const; // item index, or -1
    // The visible row at a y, whatever the x (for a held drag), or -1.
    int listRowAtY(int relative_y) const;
    // While a press is held with the pointer above or below the rows, step
    // the highlight (and scroll) that way on a timer. Runs once a frame.
    void autoScrollTick();

    // Mouse handling for the open list, in coordinates relative to the control.
    bool listMouseDown(const SDL_MouseButtonEvent& event, int rx, int ry);
    void listMouseMotion(const SDL_MouseMotionEvent& event, int rx, int ry);
    void listMouseUp(const SDL_MouseButtonEvent& event, int rx, int ry);

    Font* m_font;
    std::vector<TagLib::String> m_items;
    int m_selected = -1;
    std::function<void(int)> m_on_change;

    // Open-list state.
    int m_hot = -1;               // highlighted row
    bool m_hot_ants = false;      // it wears the focus ants: reached by keyboard
    int m_top = 0;                // first visible row
    bool m_list_above = false;    // opened upward (no room below)
    bool m_tracking = false;      // the press that opened it is still held
    bool m_list_pressed = false;  // a press in the list is held
    bool m_drag_entered = false;  // a held press has moved the highlight
    int m_drag_y = 0;             // pointer y (control-relative) while held
    Uint64 m_last_autoscroll_ms = 0;
    bool m_scrollbar_pressed = false;
    std::unique_ptr<ScrollbarWidget> m_scrollbar;
    std::unique_ptr<Surface> m_list_surface;

    Rect m_screen_pos;            // where the control was last drawn
    int m_screen_height = 0;      // height of the surface it was drawn on
    bool m_focused = false;

    static ComboBoxWidget* s_focused_widget;
    static ComboBoxWidget* s_open_widget;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // COMBOBOXWIDGET_H
