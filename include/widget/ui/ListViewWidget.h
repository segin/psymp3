/*
 * ListViewWidget.h - Scrollable list of text items
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef LISTVIEWWIDGET_H
#define LISTVIEWWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// Bring Foundation widgets into this namespace for inheritance.
using PsyMP3::Widget::Foundation::Widget;
using PsyMP3::Widget::Foundation::DrawableWidget;

/**
 * @brief A vertically scrolling list of selectable text rows.
 *
 * Rows are drawn into the widget's own surface; a child ScrollbarWidget on the
 * right edge scrolls the view when the item count exceeds what fits. Exactly one
 * row may be selected at a time. The widget re-lays out (scrollbar geometry,
 * visible-row count, scroll clamping) whenever it is resized, so a container can
 * grow/shrink it freely — call onResize(new_w, new_h) from the resize handler.
 */
class ListViewWidget : public DrawableWidget {
public:
    ListViewWidget(int width, int height, Core::Font* font);
    ~ListViewWidget() override;

    // Item management. Indices are 0-based into the current item order.
    void addItem(const TagLib::String& text);
    // Replace all items. By default the scroll resets to the top; pass
    // preserve_scroll to keep the current scroll offset (clamped) — used when
    // refreshing a list in place so the viewport doesn't jump.
    void setItems(const std::vector<TagLib::String>& items, bool preserve_scroll = false);
    void clearItems();
    size_t itemCount() const { return m_items.size(); }

    // Selection. getSelectedIndex() returns -1 when nothing is selected.
    int getSelectedIndex() const { return m_selected; }
    void setSelectedIndex(int index);
    // Scroll so the given row is within the visible area (no-op if already shown).
    void ensureVisible(int index);
    void setOnSelectionChanged(std::function<void(int)> cb) { m_on_selection_changed = std::move(cb); }
    // Fired when a row is double-clicked (the row index).
    void setOnActivate(std::function<void(int)> cb) { m_on_activate = std::move(cb); }
    // Fired when a row is drag-reordered: (from index, to index) after adjusting
    // for the removal, so it maps directly onto a move(from, to) operation.
    void setOnReorder(std::function<void(int from, int to)> cb) { m_on_reorder = std::move(cb); }
    // Fired on right-click of a row: (row, x, y) with x/y relative to this widget.
    void setOnContextMenu(std::function<void(int row, int x, int y)> cb) { m_on_context = std::move(cb); }

    // Editing helpers operating on the current selection. Each is a no-op when
    // the operation is not possible (nothing selected, already at an end, etc.)
    // and keeps the moved/adjacent item selected and visible.
    void removeSelected();
    void moveSelectedUp();
    void moveSelectedDown();

    // Resize the list in place (position unchanged), re-laying out the scrollbar
    // and clamping the scroll offset. Call this from a container's resize handler.
    void resize(int new_width, int new_height);

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseWheel(int delta, int relative_x, int relative_y) override;

protected:
    void draw(Surface& surface) override;
    void onResize(int new_width, int new_height) override;

private:
    static constexpr int SCROLLBAR_WIDTH = 16;
    static constexpr int BORDER = 1;          // sunken frame thickness
    static constexpr int ROW_PADDING = 2;     // extra pixels added to glyph height

    // Geometry helpers, all derived from the current size.
    int listAreaWidth() const;                // content width, excluding scrollbar
    int listAreaHeight() const;               // content height, excluding borders
    int visibleRows() const;                  // whole rows that fit in the area
    int maxTop() const;                       // largest valid m_top

    void relayout();          // reposition/resize the scrollbar and clamp scroll
    void syncScrollbar();     // push m_top -> scrollbar value / enabled state
    void setTop(int top);     // scroll so the given row is first, clamped
    int  rowAt(int relative_y) const; // item index under a y coordinate, or -1
    int  gapAt(int relative_y) const; // insertion gap index (0..count) for a drag

    Core::Font* m_font;
    std::vector<TagLib::String> m_items;
    int m_selected;           // -1 = none
    int m_top;                // index of the first visible row
    int m_row_height;
    ScrollbarWidget* m_scrollbar; // owned via addChild(); non-owning pointer
    std::function<void(int)> m_on_selection_changed;
    std::function<void(int)> m_on_activate;
    std::function<void(int, int)> m_on_reorder;
    std::function<void(int, int, int)> m_on_context;

    // Drag-to-reorder state. m_drag_from is the grabbed row (-1 when not
    // dragging); m_dragging becomes true once the pointer passes a small
    // threshold, and m_drag_gap is the insertion gap the drop marker shows.
    int m_drag_from = -1;
    int m_drag_start_y = 0;
    bool m_dragging = false;
    int m_drag_gap = -1;

    // Double-click detection for row activation.
    static constexpr Uint32 DOUBLE_CLICK_MS = 500;
    Uint32 m_last_click_ms = 0;
    int m_last_click_row = -1;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // LISTVIEWWIDGET_H
