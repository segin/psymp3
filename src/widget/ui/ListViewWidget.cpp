/*
 * ListViewWidget.cpp - Scrollable list of text items
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

ListViewWidget* ListViewWidget::s_focused_widget = nullptr;

ListViewWidget::ListViewWidget(int width, int height, Core::Font* font)
    : DrawableWidget(width, height)
    , m_font(font)
    , m_selected(-1)
    , m_top(0)
    , m_row_height(16)
    , m_scrollbar(nullptr)
{
    // Derive the row height from the font so rows fit the glyphs at any point
    // size; fall back to a sane default if the font can't render.
    if (m_font && m_font->isValid()) {
        auto sample = m_font->RenderLCD(TagLib::String(" Agy"), 0, 0, 0, 255, 255, 255);
        if (sample && sample->height() > 0) {
            m_row_height = sample->height() + ROW_PADDING;
        }
    }

    auto scrollbar = std::make_unique<ScrollbarWidget>(SCROLLBAR_WIDTH, height - 2 * BORDER,
                                                       ScrollbarOrientation::Vertical);
    m_scrollbar = scrollbar.get();
    m_scrollbar->setValue(0.0);
    m_scrollbar->setOnChange([this](double value) {
        // Map the scrollbar's 0..1 position onto the valid top-row range. Do NOT
        // call syncScrollbar() here: setValue() would re-enter this callback.
        int mt = maxTop();
        int new_top = (mt > 0) ? static_cast<int>(std::lround(value * mt)) : 0;
        new_top = std::max(0, std::min(new_top, mt));
        if (new_top != m_top) {
            m_top = new_top;
            invalidate();
        }
    });
    addChild(std::move(scrollbar));

    relayout();
}

ListViewWidget::~ListViewWidget()
{
    // Keyboard focus falls back to the main program when the focused list dies.
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
}

void ListViewWidget::focus()
{
    if (s_focused_widget == this) {
        return;
    }
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
    s_focused_widget = this;
    invalidate(); // show the focus dots
}

void ListViewWidget::blur()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
        invalidate(); // hide the focus dots
    }
}

void ListViewWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

bool ListViewWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    if (!s_focused_widget) {
        return false;
    }
    ListViewWidget& w = *s_focused_widget;
    if (w.m_items.empty()) {
        return false;
    }
    switch (keysym.sym) {
        case SDLK_UP:
        case SDLK_DOWN: {
            int sel = w.m_selected;
            if (sel < 0) {
                sel = w.m_top; // no cursor yet: start on the top visible row
            } else {
                sel += (keysym.sym == SDLK_DOWN) ? 1 : -1;
            }
            sel = std::max(0, std::min(sel, static_cast<int>(w.m_items.size()) - 1));
            // Shift extends the selection from its anchor; otherwise the
            // cursor moves alone. Either no-ops at the ends and, via
            // ensureVisible(), scrolls exactly one row when the cursor crosses
            // a viewport edge.
            if ((keysym.mod & SDL_KMOD_SHIFT) != 0 && w.m_anchor >= 0) {
                w.setSelectionRange(w.m_anchor, sel);
            } else {
                w.setSelectedIndex(sel);
            }
            return true;
        }
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Enter activates the cursor row, exactly like a double-click (in
            // the Playlist Manager that jumps playback to the track).
            if (w.m_selected >= 0 && w.m_on_activate) {
                auto on_activate = w.m_on_activate;
                on_activate(w.m_selected);
            }
            return true;
        case SDLK_DELETE:
            // Delete removes the selected rows (in the Playlist Manager, the
            // same action as its Delete button).
            if (w.m_selected >= 0 && w.m_on_delete) {
                auto on_delete = w.m_on_delete;
                on_delete(w.getSelectionFirst(), w.getSelectionLast());
            }
            return true;
        default:
            return false;
    }
}

int ListViewWidget::listAreaWidth() const
{
    return std::max(0, getPos().width() - 2 * BORDER - SCROLLBAR_WIDTH);
}

int ListViewWidget::listAreaHeight() const
{
    return std::max(0, getPos().height() - 2 * BORDER);
}

int ListViewWidget::visibleRows() const
{
    if (m_row_height <= 0) return 0;
    return listAreaHeight() / m_row_height;
}

int ListViewWidget::maxTop() const
{
    return std::max(0, static_cast<int>(m_items.size()) - visibleRows());
}

void ListViewWidget::relayout()
{
    if (m_scrollbar) {
        m_scrollbar->setGeometry(Rect(getPos().width() - BORDER - SCROLLBAR_WIDTH, BORDER,
                                      SCROLLBAR_WIDTH, std::max(2 * SCROLLBAR_WIDTH, listAreaHeight())));
    }
    // A resize can leave the previous top scrolled past the new end.
    m_top = std::min(m_top, maxTop());
    m_top = std::max(0, m_top);
    syncScrollbar();
}

void ListViewWidget::syncScrollbar()
{
    if (!m_scrollbar) return;
    int mt = maxTop();
    // Nothing to scroll when every item fits: park the thumb and disable it.
    m_scrollbar->setEnabled(mt > 0);
    m_scrollbar->setValue(mt > 0 ? static_cast<double>(m_top) / static_cast<double>(mt) : 0.0);
    // Value spans [0, mt] rows: an arrow click moves one row and a track click
    // one visible page, so the jump matches the list regardless of its length.
    if (mt > 0) {
        double line = 1.0 / static_cast<double>(mt);
        double page = std::min(1.0, static_cast<double>(std::max(1, visibleRows())) / static_cast<double>(mt));
        m_scrollbar->setSteps(line, page);
    }
}

void ListViewWidget::setTop(int top)
{
    top = std::max(0, std::min(top, maxTop()));
    if (top != m_top) {
        m_top = top;
        invalidate();
    }
    syncScrollbar();
}

void ListViewWidget::addItem(const TagLib::String& text)
{
    m_items.push_back(text);
    relayout();
    invalidate();
}

void ListViewWidget::setItems(const std::vector<TagLib::String>& items, bool preserve_scroll)
{
    m_items = items;
    m_selected = -1;
    m_anchor = -1;
    if (!preserve_scroll) {
        m_top = 0;
    }
    relayout(); // clamps m_top to the new maxTop()
    invalidate();
}

void ListViewWidget::clearItems()
{
    m_items.clear();
    m_selected = -1;
    m_anchor = -1;
    m_top = 0;
    relayout();
    invalidate();
}

int ListViewWidget::getSelectionFirst() const
{
    return m_selected < 0 ? -1 : std::min(m_anchor, m_selected);
}

int ListViewWidget::getSelectionLast() const
{
    return m_selected < 0 ? -1 : std::max(m_anchor, m_selected);
}

int ListViewWidget::getSelectionCount() const
{
    return m_selected < 0 ? 0 : getSelectionLast() - getSelectionFirst() + 1;
}

bool ListViewWidget::isRowSelected(int index) const
{
    return m_selected >= 0 && index >= getSelectionFirst() && index <= getSelectionLast();
}

void ListViewWidget::setSelectedIndex(int index, bool ensure_visible)
{
    setSelectionRange(index, index, ensure_visible);
}

void ListViewWidget::setSelectionRange(int anchor, int cursor, bool ensure_visible)
{
    const int count = static_cast<int>(m_items.size());
    if (cursor < 0 || cursor >= count) {
        anchor = -1;
        cursor = -1;
    } else {
        anchor = std::max(0, std::min(anchor, count - 1));
    }
    if (anchor == m_anchor && cursor == m_selected) {
        return;
    }
    m_anchor = anchor;
    m_selected = cursor;
    if (ensure_visible) {
        ensureVisible(m_selected);
    }
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

void ListViewWidget::ensureVisible(int index)
{
    if (index < 0) return;
    if (index < m_top) {
        setTop(index);
    } else if (index >= m_top + visibleRows()) {
        setTop(index - visibleRows() + 1);
    }
}

void ListViewWidget::removeSelected()
{
    const int first = getSelectionFirst();
    const int last = getSelectionLast();
    if (first < 0 || last >= static_cast<int>(m_items.size())) {
        return;
    }
    m_items.erase(m_items.begin() + first, m_items.begin() + last + 1);

    // Select the single row now in the first removed slot (the item after the
    // block); past the end, the new last row; nothing when the list is empty.
    m_selected = m_items.empty() ? -1 : std::min(first, static_cast<int>(m_items.size()) - 1);
    m_anchor = m_selected;

    relayout();
    ensureVisible(m_selected);
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

void ListViewWidget::moveSelectedUp()
{
    const int first = getSelectionFirst();
    const int last = getSelectionLast();
    if (first <= 0 || last >= static_cast<int>(m_items.size())) {
        return;
    }
    // The row above the block moves to just below it.
    std::rotate(m_items.begin() + first - 1, m_items.begin() + first, m_items.begin() + last + 1);
    m_anchor -= 1;
    m_selected -= 1;
    ensureVisible(m_selected);
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

void ListViewWidget::moveSelectedDown()
{
    const int first = getSelectionFirst();
    const int last = getSelectionLast();
    if (first < 0 || last >= static_cast<int>(m_items.size()) - 1) {
        return;
    }
    // The row below the block moves to just above it.
    std::rotate(m_items.begin() + first, m_items.begin() + last + 1, m_items.begin() + last + 2);
    m_anchor += 1;
    m_selected += 1;
    ensureVisible(m_selected);
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

bool ListViewWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Let the scrollbar child (and any future children) have first refusal.
    if (Widget::handleMouseDown(event, relative_x, relative_y)) {
        return true;
    }

    const bool in_rows = (relative_x >= BORDER && relative_x < BORDER + listAreaWidth() &&
                          relative_y >= BORDER && relative_y < BORDER + listAreaHeight());

    // Right-click a row: raise the context menu at the cursor. A row inside the
    // selection keeps it, so the menu acts on every selected row; any other
    // row is selected first.
    if (event.button == SDL_BUTTON_RIGHT && isEnabled() && in_rows) {
        focus();
        int row = rowAt(relative_y);
        if (row >= 0) {
            if (!isRowSelected(row)) {
                setSelectedIndex(row);
            }
            if (m_on_context) m_on_context(row, relative_x, relative_y);
            return true;
        }
        return false;
    }

    if (event.button != SDL_BUTTON_LEFT || !isEnabled()) {
        return false;
    }

    // Clicks inside the row area select the row under the cursor; a second click
    // on the same row within the double-click window activates it. Shift+click
    // extends the selection from its anchor to the clicked row instead.
    if (relative_x >= BORDER && relative_x < BORDER + listAreaWidth() &&
        relative_y >= BORDER && relative_y < BORDER + listAreaHeight()) {
        focus();
        int row = rowAt(relative_y);
        if (row >= 0) {
            const bool extend = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0 && m_selected >= 0;
            Uint32 now = SDL_GetTicks();
            if (!extend && row == m_last_click_row && (now - m_last_click_ms) <= DOUBLE_CLICK_MS) {
                m_last_click_ms = 0; // consume, so a third click isn't a double
                m_last_click_row = -1;
                if (m_on_activate) m_on_activate(row);
            } else {
                if (extend) {
                    setSelectionRange(m_anchor, row);
                    m_last_click_row = -1; // a range click is never half a double-click
                    m_last_click_ms = 0;
                } else if (getSelectionCount() > 1 && isRowSelected(row)) {
                    // Keep the block selected so it can be dragged as one; a
                    // release without dragging selects just this row.
                    m_collapse_on_release = row;
                    m_last_click_row = row;
                    m_last_click_ms = now;
                } else {
                    setSelectedIndex(row);
                    m_last_click_row = row;
                    m_last_click_ms = now;
                }
                // Begin a potential drag of the selected rows (only meaningful
                // with 2+ rows); it becomes a real drag once the pointer passes
                // a threshold.
                if (m_items.size() >= 2) {
                    m_drag_first = getSelectionFirst();
                    m_drag_last = getSelectionLast();
                    m_drag_start_y = relative_y;
                    m_dragging = false;
                    m_drag_gap = -1;
                    captureMouse();
                }
            }
        }
        return true;
    }

    return false;
}

bool ListViewWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (m_drag_first >= 0) {
        // Ignore small jitter so a plain click doesn't register as a drag.
        if (!m_dragging && std::abs(relative_y - m_drag_start_y) < m_row_height / 2) {
            return true;
        }
        m_dragging = true;
        m_collapse_on_release = -1; // a drag, not a click
        // Above/below the rows: arm the edge auto-scroll (speed follows the
        // pointer's current distance past the edge; see autoScrollTick()) and
        // pin the marker to the visible boundary instead of a hidden gap. Over
        // the dragged block itself there is nowhere to drop, so no marker.
        updateScrollZone(relative_y);
        int gap = dropGapOutsideBlock((m_scroll_zone == 0) ? gapAt(relative_y) : edgeGap());
        if (gap != m_drag_gap) {
            m_drag_gap = gap;
            invalidate();
        }
        return true;
    }
    return Widget::handleMouseMotion(event, relative_x, relative_y);
}

bool ListViewWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (m_drag_first >= 0) {
        // A drag-reorder is a LEFT-button gesture: releases of other buttons
        // (routed here via the capture) must not commit the reorder and drop
        // the capture while the left button is still physically held.
        if (event.button != SDL_BUTTON_LEFT) {
            return true; // swallow the stray release; the gesture continues
        }
        releaseMouse();
        const int first = m_drag_first;
        const int last = m_drag_last;
        const bool dragged = m_dragging;
        const int gap = m_drag_gap;
        const int collapse = m_collapse_on_release;
        m_drag_first = -1;
        m_drag_last = -1;
        m_dragging = false;
        m_drag_gap = -1;
        m_collapse_on_release = -1;
        m_scroll_zone = 0;
        m_scroll_distance = 0;
        invalidate();
        if (dragged) {
            // gap is an insertion slot (0..count) outside the block; one inside
            // it was refused as it was hovered. Taking the block out shifts
            // every slot after it up by the block's length, which gives the
            // index its first row lands on.
            const int size = last - first + 1;
            const int to = (gap > last) ? gap - size : gap;
            if (gap >= 0 && m_on_reorder && to != first && to >= 0 &&
                to + size <= static_cast<int>(m_items.size())) {
                m_on_reorder(first, last, to);
            }
        } else if (collapse >= 0) {
            setSelectedIndex(collapse, /*ensure_visible=*/false);
        }
        return true;
    }
    return Widget::handleMouseUp(event, relative_x, relative_y);
}

void ListViewWidget::cancelDrag()
{
    if (m_drag_first < 0) {
        return;
    }
    releaseMouse(); // no-op if we don't hold capture
    m_drag_first = -1;
    m_drag_last = -1;
    m_collapse_on_release = -1;
    m_dragging = false;
    m_drag_gap = -1;
    m_scroll_zone = 0;
    m_scroll_distance = 0;
    invalidate();
}

void ListViewWidget::setDropIndicator(int gap)
{
    if (gap < 0) {
        // The external drag ended or left the list: stop any edge auto-scroll.
        m_scroll_zone = 0;
        m_scroll_distance = 0;
    }
    if (gap == m_drop_indicator) {
        return;
    }
    m_drop_indicator = gap;
    invalidate();
}

void ListViewWidget::updateScrollZone(int relative_y)
{
    const int top_edge = BORDER;
    const int bottom_edge = BORDER + listAreaHeight();
    if (relative_y < top_edge) {
        m_scroll_zone = -1;
        m_scroll_distance = top_edge - relative_y;
    } else if (relative_y >= bottom_edge) {
        m_scroll_zone = 1;
        m_scroll_distance = relative_y - bottom_edge + 1;
    } else {
        m_scroll_zone = 0;
        m_scroll_distance = 0;
    }
}

int ListViewWidget::edgeGap() const
{
    // The insertion gap at the visible boundary the auto-scroll is crossing.
    return (m_scroll_zone < 0)
        ? m_top
        : std::min(m_top + visibleRows(), static_cast<int>(m_items.size()));
}

int ListViewWidget::externalDropHover(int relative_x, int relative_y)
{
    if (relative_x < 0 || relative_x >= getPos().width()) {
        m_scroll_zone = 0;
        m_scroll_distance = 0;
        return -1;
    }
    updateScrollZone(relative_y);
    if (m_scroll_zone == 0) {
        return gapAt(relative_y);
    }
    // Beyond an edge: pin the insertion gap to the visible boundary; the
    // auto-scroll tick keeps it pinned as the content crawls past.
    return edgeGap();
}

void ListViewWidget::autoScrollTick()
{
    if (m_scroll_zone == 0 || m_items.empty()) {
        return;
    }
    // The interval follows the pointer's CURRENT distance past the edge: just
    // past it crawls (~6 rows/s), and it accelerates smoothly to a cap of about
    // one row per 30 ms (~33 rows/s) at 65px out — moving the pointer back
    // toward the edge slows the crawl again.
    const Uint32 interval = static_cast<Uint32>(std::max(30, 160 - 2 * m_scroll_distance));
    const Uint32 now = SDL_GetTicks();
    if (now - m_last_autoscroll_ms < interval) {
        return;
    }
    m_last_autoscroll_ms = now;
    const int old_top = m_top;
    setTop(m_top + m_scroll_zone);
    if (m_top == old_top) {
        return; // already at the end in this direction
    }
    // Keep the active insertion marker pinned to the boundary gap.
    if (m_dragging) {
        m_drag_gap = dropGapOutsideBlock(edgeGap());
    } else if (m_drop_indicator >= 0) {
        m_drop_indicator = edgeGap();
    }
    invalidate();
}

void ListViewWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    // Rendering runs once per frame, so it doubles as the auto-scroll clock:
    // the list keeps crawling while the drag pointer holds still past an edge
    // (neither mouse-motion nor drop-position events arrive without movement).
    autoScrollTick();
    DrawableWidget::recursiveBlitTo(target, parent_absolute_pos);
}

int ListViewWidget::rowAt(int relative_y) const
{
    if (relative_y < BORDER || m_row_height <= 0) {
        return -1;
    }
    int r = m_top + (relative_y - BORDER) / m_row_height;
    return (r >= 0 && r < static_cast<int>(m_items.size())) ? r : -1;
}

int ListViewWidget::dropGapOutsideBlock(int gap) const
{
    // Every gap from the block's first row to just past its last leaves the
    // block where it is -- and dropping a block into the middle of itself has
    // no meaning -- so none of them is a drop target.
    return (m_drag_first >= 0 && gap >= m_drag_first && gap <= m_drag_last + 1) ? -1 : gap;
}

int ListViewWidget::gapAt(int relative_y) const
{
    if (m_row_height <= 0) {
        return 0;
    }
    int rel = std::max(0, relative_y - BORDER);
    int gap = m_top + (rel + m_row_height / 2) / m_row_height;
    return std::max(0, std::min(gap, static_cast<int>(m_items.size())));
}

bool ListViewWidget::handleMouseWheel(int delta, int relative_x, int relative_y)
{
    (void)relative_x;
    (void)relative_y;
    if (!isEnabled() || m_items.empty()) {
        return false;
    }
    // Scroll three rows per wheel notch; positive delta (wheel up) shows earlier
    // rows. setTop() clamps and repaints.
    const int kLinesPerNotch = 3;
    int new_top = m_top - delta * kLinesPerNotch;
    if (new_top == m_top) {
        return true;
    }
    setTop(new_top);
    return true;
}

void ListViewWidget::resize(int new_width, int new_height)
{
    onResize(new_width, new_height);
}

void ListViewWidget::onResize(int new_width, int new_height)
{
    setPos(Rect(getPos().x(), getPos().y(), new_width, new_height));
    relayout();
    redraw();
}

void ListViewWidget::draw(Surface& surface)
{
    // White background.
    surface.FillRect(surface.MapRGB(255, 255, 255));

    const int w = getPos().width();
    const int h = getPos().height();
    const int content_w = listAreaWidth();

    // Draw the visible rows.
    const int rows = visibleRows();
    for (int i = 0; i < rows; ++i) {
        int index = m_top + i;
        if (index >= static_cast<int>(m_items.size())) {
            break;
        }
        int row_y = BORDER + i * m_row_height;
        bool selected = isRowSelected(index);

        if (selected) {
            surface.box(BORDER, row_y, BORDER + content_w - 1, row_y + m_row_height - 1,
                        0, 0, 128, 255);
        }

        if (m_font && m_font->isValid() && !m_items[index].isEmpty()) {
            std::unique_ptr<Surface> text = selected
                ? m_font->RenderLCD(m_items[index], 255, 255, 255, 0, 0, 128)
                : m_font->RenderLCD(m_items[index], 0, 0, 0, 255, 255, 255);
            if (text && text->width() > 0) {
                int ty = row_y + (m_row_height - text->height()) / 2;
                // SDL clips the blit to this surface; the scrollbar column then
                // covers any overrun past the content width.
                surface.Blit(*text, Rect(BORDER + 2, ty, text->width(), text->height()));
            }
        }

        // Classic keyboard-focus rectangle: while this list holds keyboard
        // focus, the cursor row gets a 1px dotted outline over its highlight.
        if (index == m_selected && s_focused_widget == this) {
            const int x0 = BORDER;
            const int x1 = BORDER + content_w - 1;
            const int y0 = row_y;
            const int y1 = row_y + m_row_height - 1;
            for (int x = x0; x <= x1; ++x) {
                if (((x + y0) & 1) == 0) surface.pixel(x, y0, 255, 255, 255, 255);
                if (((x + y1) & 1) == 0) surface.pixel(x, y1, 255, 255, 255, 255);
            }
            for (int y = y0 + 1; y < y1; ++y) {
                if (((x0 + y) & 1) == 0) surface.pixel(x0, y, 255, 255, 255, 255);
                if (((x1 + y) & 1) == 0) surface.pixel(x1, y, 255, 255, 255, 255);
            }
        }
    }

    // Insertion marker (2px blue line at the gap): shown for an internal
    // drag-to-reorder and, identically, for an external file drag hovering over
    // the list (m_drop_indicator, set via setDropIndicator()).
    const int marker_gap = m_dragging ? m_drag_gap : m_drop_indicator;
    if (marker_gap >= m_top && marker_gap <= m_top + rows) {
        int my = BORDER + (marker_gap - m_top) * m_row_height;
        int y0 = std::min(my, h - BORDER - 2);
        surface.hline(BORDER, BORDER + content_w - 1, y0, 0, 0, 200, 255);
        surface.hline(BORDER, BORDER + content_w - 1, y0 + 1, 0, 0, 200, 255);
    }

    // Sunken 3D frame (dark top/left, light bottom/right) drawn last so it sits
    // above the rows at the edges.
    surface.hline(0, w - 1, 0, 128, 128, 128, 255);
    surface.vline(0, 0, h - 1, 128, 128, 128, 255);
    surface.hline(0, w - 1, h - 1, 255, 255, 255, 255);
    surface.vline(w - 1, 0, h - 1, 255, 255, 255, 255);
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
