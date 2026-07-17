/*
 * ContextMenuWidget.h - Lightweight right-click popup menu
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
 * @brief A standalone right-click popup menu.
 *
 * Sized to fill its container and added on top of the other children: while
 * closed it is transparent and passes clicks through (returns false); while
 * open it draws a Win3.1-style popup at a stored point, highlights the hovered
 * item, and activates an item (or dismisses on an outside click) on press.
 * Disabled entries are greyed and inert.
 */
class ContextMenuWidget : public DrawableWidget {
public:
    struct Entry {
        std::string label;
        std::function<void()> action;
        bool enabled = true;
    };

    ContextMenuWidget(int width, int height, Core::Font* font);

    // Replace the menu contents (and recompute the popup width).
    void setEntries(std::vector<Entry> entries);
    // Show the popup with its top-left near (x, y) in this widget's coordinates.
    void openAt(int x, int y);
    void close();
    bool isOpen() const { return m_open; }
    // Resize the full-window overlay to a new container size.
    void resize(int width, int height) { onResize(width, height); }

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;

protected:
    void draw(Surface& surface) override;

private:
    static constexpr int ITEM_H = 18;   // item row height
    static constexpr int PAD = 6;        // horizontal text padding

    Rect popupRect() const;              // clamped to stay within the widget
    int itemAt(int x, int y) const;      // entry index under a point, or -1

    Core::Font* m_font;
    std::vector<Entry> m_entries;
    bool m_open = false;
    int m_x = 0;
    int m_y = 0;
    int m_hover = -1;
    int m_width_px = 40;                 // computed from the entry labels
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // CONTEXTMENUWIDGET_H
