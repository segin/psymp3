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
    // Derive the row height from the font's line height, as the combo box's
    // list does, so the two lists' rows match; fall back to a sane default if
    // the font can't render.
    if (m_font && m_font->isValid() && m_font->lineHeight() > 0) {
        m_row_height = m_font->lineHeight() + ROW_PADDING;
    }

    auto scrollbar = std::make_unique<ScrollbarWidget>(SCROLLBAR_WIDTH, height,
                                                       ScrollbarOrientation::Vertical);
    m_scrollbar = scrollbar.get();
    m_scrollbar->setValue(0.0);
    m_scrollbar->setOnChange([this](double value) {
        // Map the scrollbar's 0..1 position onto the pixel range. Do NOT
        // sync the scrollbar back here: setValue() would re-enter this callback.
        setScrollPx(static_cast<int>(std::lround(value * maxScrollPx())), /*sync_scrollbar=*/false);
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
        // Nothing to move through, but the list still has focus: the owner's
        // keys (the Playlist Manager's Ctrl+C, Ctrl+Up/Down) and the cursor
        // keys stay the list's, doing nothing, rather than reach the global
        // shortcuts (Up/Down change the volume, C the spectrum decay).
        if (w.m_on_key) {
            auto on_key = w.m_on_key; // the callback may replace it
            if (on_key(keysym)) {
                return true;
            }
        }
        switch (keysym.sym) {
            case SDLK_UP: case SDLK_DOWN: case SDLK_PAGEUP: case SDLK_PAGEDOWN:
            case SDLK_HOME: case SDLK_END:
                return true;
            default:
                return false;
        }
    }
    // A key during a held drag can change the rows or the selection (Delete,
    // Ctrl+Up/Down, the cursor keys): the drag's recorded block would then
    // name other rows, and its release would move the wrong ones. Abandon
    // the drag, unless the key is a bare modifier.
    if (w.m_drag_first >= 0) {
        switch (keysym.sym) {
            case SDLK_LSHIFT: case SDLK_RSHIFT: case SDLK_LCTRL: case SDLK_RCTRL:
            case SDLK_LALT: case SDLK_RALT: case SDLK_LGUI: case SDLK_RGUI:
                break;
            default:
                w.cancelDrag();
                break;
        }
    }
    if (w.m_on_key) {
        auto on_key = w.m_on_key; // the callback may replace it
        if (on_key(keysym)) {
            return true;
        }
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
        case SDLK_PAGEUP:
        case SDLK_PAGEDOWN: {
            // A page at a time: the cursor moves by one row less than fit, so
            // the row it leaves stays in view, as in a Windows list box.
            // Shift extends the selection, like Shift+Up/Down.
            const int page = std::max(1, w.visibleRows() - 1);
            int sel = w.m_selected < 0 ? w.m_top : w.m_selected;
            sel += (keysym.sym == SDLK_PAGEDOWN) ? page : -page;
            sel = std::max(0, std::min(sel, static_cast<int>(w.m_items.size()) - 1));
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
    // From the left border to the scrollbar, whose left outline ends the rows.
    return std::max(0, getPos().width() - BORDER - SCROLLBAR_WIDTH);
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

int ListViewWidget::maxScrollPx() const
{
    // Scrolled all the way, the last row's bottom meets the area's bottom.
    return std::max(0, static_cast<int>(m_items.size()) * m_row_height - listAreaHeight());
}

void ListViewWidget::relayout()
{
    if (m_scrollbar) {
        // Over the border on the right, top and bottom, as in Windows 3.1:
        // the scrollbar's own black outline is the list's frame there.
        m_scrollbar->setGeometry(Rect(getPos().width() - SCROLLBAR_WIDTH, 0,
                                      SCROLLBAR_WIDTH, std::max(2 * SCROLLBAR_WIDTH, static_cast<int>(getPos().height()))));
    }
    // A resize (or new items) can leave the offset scrolled past the new end.
    setScrollPx(m_scroll_px);
}

void ListViewWidget::syncScrollbar()
{
    if (!m_scrollbar) return;
    const int max_px = maxScrollPx();
    // Nothing to scroll when every item fits: park the thumb and disable it.
    m_scrollbar->setEnabled(max_px > 0);
    m_scrollbar->setValue(max_px > 0 ? static_cast<double>(m_scroll_px) / static_cast<double>(max_px) : 0.0);
    // Value spans [0, max_px] pixels: an arrow click moves one row and a
    // track click one area's height, whatever the list's length.
    if (max_px > 0) {
        const double line = std::min(1.0, static_cast<double>(m_row_height) / max_px);
        const double page = std::min(1.0, static_cast<double>(std::max(1, listAreaHeight())) / max_px);
        m_scrollbar->setSteps(line, page);
    }
}

void ListViewWidget::setScrollPx(int px, bool sync_scrollbar)
{
    px = std::max(0, std::min(px, maxScrollPx()));
    if (px != m_scroll_px) {
        m_scroll_px = px;
        invalidate();
    }
    m_top = (m_row_height > 0) ? m_scroll_px / m_row_height : 0;
    if (sync_scrollbar) {
        syncScrollbar();
    }
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
        m_scroll_px = 0;
    }
    relayout(); // clamps the offset to the new maxScrollPx()
    invalidate();
}

void ListViewWidget::clearItems()
{
    m_items.clear();
    m_selected = -1;
    m_anchor = -1;
    m_scroll_px = 0;
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

void ListViewWidget::centerOn(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size())) {
        return;
    }
    // The row's middle at the area's middle; setScrollPx clamps at the ends.
    setScrollPx(index * m_row_height + m_row_height / 2 - listAreaHeight() / 2);
}

void ListViewWidget::ensureVisible(int index)
{
    if (index < 0 || m_row_height <= 0) return;
    // Scroll just far enough to show the whole row: revealed from above it
    // comes in at the top edge, from below at the bottom edge (the row at
    // the top then shows partly). A row already whole in view stays put.
    const int row_top = index * m_row_height;
    const int row_bottom = row_top + m_row_height;
    const int area = listAreaHeight();
    if (row_top < m_scroll_px) {
        setScrollPx(row_top);
    } else if (row_bottom > m_scroll_px + area) {
        setScrollPx(row_bottom - area);
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
    const int old_px = m_scroll_px;
    setScrollPx(m_scroll_px + m_scroll_zone * m_row_height);
    if (m_scroll_px == old_px) {
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

namespace {
// Division rounding toward negative infinity, for offsets above row m_top.
int floorDiv(int a, int b)
{
    return (a >= 0) ? a / b : -((-a + b - 1) / b);
}
} // namespace

int ListViewWidget::rowOriginY() const
{
    // Row m_top starts at or above the top edge, by however much of it the
    // pixel offset has scrolled away.
    return (m_row_height > 0) ? BORDER - m_scroll_px % m_row_height : BORDER;
}

int ListViewWidget::rowAt(int relative_y) const
{
    if (relative_y < BORDER || relative_y >= BORDER + listAreaHeight() || m_row_height <= 0) {
        return -1;
    }
    // Partly shown rows count: clicking one selects it and scrolls it whole.
    const int r = m_top + floorDiv(relative_y - rowOriginY(), m_row_height);
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
    // The row boundary nearest the pointer, measured from where rows start,
    // kept to the boundaries inside the list area: those of a partly shown
    // row lie past its edge, where no insertion marker could be seen.
    const int origin = rowOriginY();
    const int first = -floorDiv(origin - BORDER, m_row_height);                  // ceil
    const int last = floorDiv(BORDER + listAreaHeight() - origin, m_row_height); // the bottom edge counts
    int gap = floorDiv(relative_y - origin + m_row_height / 2, m_row_height);
    gap = m_top + std::max(first, std::min(gap, last));
    return std::max(0, std::min(gap, static_cast<int>(m_items.size())));
}

bool ListViewWidget::handleMouseWheel(int delta, int relative_x, int relative_y)
{
    (void)relative_x;
    (void)relative_y;
    if (!isEnabled() || m_items.empty()) {
        return false;
    }
    // Scroll three rows' height per wheel notch, from wherever the offset is
    // (no snapping to rows); positive delta (wheel up) shows earlier rows.
    // setScrollPx() clamps and repaints.
    const int kLinesPerNotch = 3;
    setScrollPx(m_scroll_px - delta * kLinesPerNotch * m_row_height);
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

    // Draw the rows in view, partly shown ones included (see rowOriginY):
    // each into a row surface, blitted at its place, so a row hanging past
    // the top or bottom edge is clipped by the blit; the frame, drawn last,
    // covers what lands on the border. Rendering is just-in-time: only the
    // rows in view are rendered, on each redraw, into one reused row
    // surface; nothing rendered outlives the draw, so a long list costs its
    // strings and this widget's own surface, never a rendering per item.
    const int count = static_cast<int>(m_items.size());
    const int origin = rowOriginY();
    const int area_bottom = BORDER + listAreaHeight();
    std::unique_ptr<Surface> row_surface;
    if (m_row_height > 0 && content_w > 0 && count > 0) {
        row_surface = std::make_unique<Surface>(content_w, m_row_height, true);
    }
    for (int index = m_top; row_surface && index < count; ++index) {
        const int row_y = origin + (index - m_top) * m_row_height;
        if (row_y >= area_bottom) {
            break;
        }
        const bool selected = isRowSelected(index);
        const SDL_Color fill = selected ? SDL_Color{0, 0, 128, 255} : SDL_Color{255, 255, 255, 255};
        Surface& row = *row_surface;
        row.FillRect(row.MapRGBA(fill.r, fill.g, fill.b, 255));

        if (m_font && m_font->isValid() && !m_items[index].isEmpty()) {
            std::unique_ptr<Surface> text = selected
                ? m_font->RenderLCD(m_items[index], 255, 255, 255, 0, 0, 128)
                : m_font->RenderLCD(m_items[index], 0, 0, 0, 255, 255, 255);
            if (text && text->width() > 0) {
                // The blit clips any overrun past the row's width.
                row.Blit(*text, Rect(2, (m_row_height - text->height()) / 2, text->width(), text->height()));
            }
        }

        // Classic keyboard-focus rectangle: while this list holds keyboard
        // focus, the cursor row gets the focus ants over its fill (the
        // highlight's navy, or white if the row isn't selected).
        if (index == m_selected && s_focused_widget == this) {
            drawFocusAnts(row, 0, 0, content_w - 1, m_row_height - 1, fill);
        }

        surface.Blit(row, Rect(BORDER, row_y, content_w, m_row_height));
    }

    // Insertion marker (2px blue line at the gap): shown for an internal
    // drag-to-reorder and, identically, for an external file drag hovering over
    // the list (m_drop_indicator, set via setDropIndicator()). gapAt() only
    // yields boundaries inside the area; keep the line within it too.
    const int marker_gap = m_dragging ? m_drag_gap : m_drop_indicator;
    if (marker_gap >= 0 && m_row_height > 0) {
        const int my = origin + (marker_gap - m_top) * m_row_height;
        if (my >= BORDER && my <= area_bottom) {
            const int y0 = std::max(BORDER, std::min(my, h - BORDER - 2));
            surface.hline(BORDER, BORDER + content_w - 1, y0, 0, 0, 200, 255);
            surface.hline(BORDER, BORDER + content_w - 1, y0 + 1, 0, 0, 200, 255);
        }
    }

    // The flat 1px black Windows 3.1 frame, drawn last so it sits above the
    // rows at the edges. The scrollbar, drawn over the right of it, merges
    // its own outline into it.
    surface.rectangle(0, 0, w - 1, h - 1, 0, 0, 0, 255);
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
