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

ListViewWidget::~ListViewWidget() = default;

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
    m_top = 0;
    relayout();
    invalidate();
}

void ListViewWidget::setSelectedIndex(int index)
{
    if (index < -1 || index >= static_cast<int>(m_items.size())) {
        index = -1;
    }
    if (index == m_selected) {
        return;
    }
    m_selected = index;
    ensureVisible(m_selected);
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
    if (m_selected < 0 || m_selected >= static_cast<int>(m_items.size())) {
        return;
    }
    m_items.erase(m_items.begin() + m_selected);

    // Keep the same slot selected (now holding the next item); if we removed the
    // last item, fall back to the new last row, or clear when the list is empty.
    if (m_items.empty()) {
        m_selected = -1;
    } else if (m_selected >= static_cast<int>(m_items.size())) {
        m_selected = static_cast<int>(m_items.size()) - 1;
    }

    relayout();
    ensureVisible(m_selected);
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

void ListViewWidget::moveSelectedUp()
{
    if (m_selected <= 0 || m_selected >= static_cast<int>(m_items.size())) {
        return;
    }
    std::swap(m_items[m_selected - 1], m_items[m_selected]);
    m_selected -= 1;
    ensureVisible(m_selected);
    invalidate();
    if (m_on_selection_changed) {
        m_on_selection_changed(m_selected);
    }
}

void ListViewWidget::moveSelectedDown()
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_items.size()) - 1) {
        return;
    }
    std::swap(m_items[m_selected], m_items[m_selected + 1]);
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

    // Right-click a row: select it and raise the context menu at the cursor.
    if (event.button == SDL_BUTTON_RIGHT && isEnabled() && in_rows) {
        int row = rowAt(relative_y);
        if (row >= 0) {
            setSelectedIndex(row);
            if (m_on_context) m_on_context(row, relative_x, relative_y);
            return true;
        }
        return false;
    }

    if (event.button != SDL_BUTTON_LEFT || !isEnabled()) {
        return false;
    }

    // Clicks inside the row area select the row under the cursor; a second click
    // on the same row within the double-click window activates it.
    if (relative_x >= BORDER && relative_x < BORDER + listAreaWidth() &&
        relative_y >= BORDER && relative_y < BORDER + listAreaHeight()) {
        int row = rowAt(relative_y);
        if (row >= 0) {
            Uint32 now = SDL_GetTicks();
            if (row == m_last_click_row && (now - m_last_click_ms) <= DOUBLE_CLICK_MS) {
                m_last_click_ms = 0; // consume, so a third click isn't a double
                m_last_click_row = -1;
                if (m_on_activate) m_on_activate(row);
            } else {
                setSelectedIndex(row);
                m_last_click_row = row;
                m_last_click_ms = now;
                // Begin a potential drag-to-reorder; it becomes a real drag only
                // once the pointer passes a threshold (see handleMouseMotion).
                m_drag_from = row;
                m_drag_start_y = relative_y;
                m_dragging = false;
                m_drag_gap = -1;
                captureMouse();
            }
        }
        return true;
    }

    return false;
}

bool ListViewWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    if (m_drag_from >= 0) {
        // Ignore small jitter so a plain click doesn't register as a drag.
        if (!m_dragging && std::abs(relative_y - m_drag_start_y) < m_row_height / 2) {
            return true;
        }
        m_dragging = true;
        int gap = gapAt(relative_y);
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
    if (m_drag_from >= 0) {
        releaseMouse();
        int from = m_drag_from;
        bool dragged = m_dragging;
        int gap = m_drag_gap;
        m_drag_from = -1;
        m_dragging = false;
        m_drag_gap = -1;
        invalidate();
        if (dragged && m_on_reorder) {
            // gap is the insertion slot (0..count); after removing `from`, a slot
            // past it shifts down one, giving the final destination index.
            int dest = (gap > from) ? gap - 1 : gap;
            if (dest != from && dest >= 0 && dest < static_cast<int>(m_items.size())) {
                m_on_reorder(from, dest);
            }
        }
        return true;
    }
    return Widget::handleMouseUp(event, relative_x, relative_y);
}

int ListViewWidget::rowAt(int relative_y) const
{
    if (relative_y < BORDER || m_row_height <= 0) {
        return -1;
    }
    int r = m_top + (relative_y - BORDER) / m_row_height;
    return (r >= 0 && r < static_cast<int>(m_items.size())) ? r : -1;
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
        bool selected = (index == m_selected);

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
    }

    // Drag-to-reorder drop marker: a 2px line at the insertion gap.
    if (m_dragging && m_drag_gap >= m_top && m_drag_gap <= m_top + rows) {
        int my = BORDER + (m_drag_gap - m_top) * m_row_height;
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
