/*
 * ComboBoxWidget.cpp - Windows 3.1 style drop-down list (combo box)
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

ComboBoxWidget* ComboBoxWidget::s_focused_widget = nullptr;
ComboBoxWidget* ComboBoxWidget::s_open_widget = nullptr;

namespace {

constexpr SDL_Color kNavy = {0, 0, 128, 255};

// The Windows 3.1 push-button piece, as the scrollbar arrows draw it: grey
// face inside a black outline, a white top/left highlight and a stepped 2px
// grey bottom/right shadow; pressed, a grey line inside the top/left only.
// (Its own name: the unity build puts every file's helpers in one unit.)
void drawComboButton(::Surface& surface, const Rect& rect, bool pressed)
{
    const int x1 = rect.x(), y1 = rect.y();
    const int x2 = rect.x() + rect.width() - 1, y2 = rect.y() + rect.height() - 1;
    surface.box(x1, y1, x2, y2, 192, 192, 192, 255);
    surface.rectangle(x1, y1, x2, y2, 0, 0, 0, 255);
    if (pressed) {
        surface.hline(x1 + 1, x2 - 1, y1 + 1, 128, 128, 128, 255);
        surface.vline(x1 + 1, y1 + 1, y2 - 1, 128, 128, 128, 255);
    } else {
        surface.hline(x1 + 1, x2 - 2, y1 + 1, 255, 255, 255, 255);
        surface.vline(x1 + 1, y1 + 1, y2 - 2, 255, 255, 255, 255);
        surface.hline(x1 + 1, x2 - 1, y2 - 1, 128, 128, 128, 255);
        surface.hline(x1 + 2, x2 - 1, y2 - 2, 128, 128, 128, 255);
        surface.vline(x2 - 1, y1 + 1, y2 - 1, 128, 128, 128, 255);
        surface.vline(x2 - 2, y1 + 2, y2 - 2, 128, 128, 128, 255);
    }
}

// The drop-down button's glyph, pixel-counted from Windows 3.1: a 3x3 stem, a
// 7-5-3-1 head, a row's gap and a 7px bar. `cx` is the centre column and `top`
// the stem's first row.
void drawDropGlyph(::Surface& surface, int cx, int top, uint8_t shade)
{
    const uint8_t c = shade;
    for (int y = top; y < top + 3; ++y) {
        surface.hline(cx - 1, cx + 1, y, c, c, c, 255);
    }
    surface.hline(cx - 3, cx + 3, top + 3, c, c, c, 255);
    surface.hline(cx - 2, cx + 2, top + 4, c, c, c, 255);
    surface.hline(cx - 1, cx + 1, top + 5, c, c, c, 255);
    surface.pixel(cx, top + 6, c, c, c, 255);
    surface.hline(cx - 3, cx + 3, top + 8, c, c, c, 255);
}

constexpr int kGlyphHeight = 9;

} // namespace

ComboBoxWidget::ComboBoxWidget(int width, int height, Font* font)
    : Widget()
    , m_font(font)
{
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

ComboBoxWidget::~ComboBoxWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    if (s_open_widget == this) {
        s_open_widget = nullptr;
    }
}

void ComboBoxWidget::setItems(std::vector<TagLib::String> items)
{
    if (isOpen()) {
        close(false);
    }
    m_items = std::move(items);
    if (m_selected >= static_cast<int>(m_items.size())) {
        m_selected = m_items.empty() ? -1 : static_cast<int>(m_items.size()) - 1;
    }
    if (static_cast<int>(m_items.size()) > kMaxVisibleRows) {
        if (!m_scrollbar) {
            m_scrollbar = std::make_unique<ScrollbarWidget>(kScrollbarWidth, 10, ScrollbarOrientation::Vertical);
            m_scrollbar->setOnChange([this](double value) {
                const int range = static_cast<int>(m_items.size()) - visibleRows();
                const int top = range > 0 ? static_cast<int>(std::lround(value * range)) : 0;
                if (top != m_top) {
                    m_top = top;
                    rebuildList();
                }
            });
        }
    } else {
        m_scrollbar.reset();
    }
    rebuildSurface();
}

void ComboBoxWidget::setSelectedIndex(int index)
{
    if (index < -1 || index >= static_cast<int>(m_items.size())) {
        index = -1;
    }
    if (index != m_selected) {
        m_selected = index;
        rebuildSurface();
    }
}

void ComboBoxWidget::choose(int index)
{
    if (index < 0 || index >= static_cast<int>(m_items.size()) || index == m_selected) {
        return;
    }
    m_selected = index;
    rebuildSurface();
    if (m_on_change) {
        auto on_change = m_on_change; // the callback may replace it
        on_change(index);
    }
}

void ComboBoxWidget::setEnabled(bool enabled)
{
    if (!enabled && isOpen()) {
        close(false);
    }
    Widget::setEnabled(enabled);
    rebuildSurface();
}

bool ComboBoxWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    const Rect& pos = getPos();
    if (!isEnabled() || event.button != SDL_BUTTON_LEFT ||
        relative_x < 0 || relative_x >= pos.width() || relative_y < 0 || relative_y >= pos.height()) {
        return false;
    }
    focus();
    open();
    // The press that opened the list can be dragged onto an item and released
    // there to pick it; until it is released, the button stays sunk.
    m_tracking = isOpen();
    m_drag_entered = false;
    m_drag_y = relative_y;
    rebuildSurface();
    return true;
}

bool ComboBoxWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    // Releases while the list is open go to routeOpenListMouseUp.
    (void)event;
    (void)relative_x;
    (void)relative_y;
    return false;
}

bool ComboBoxWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;
    const Rect& pos = getPos();
    return relative_x >= 0 && relative_x < pos.width() && relative_y >= 0 && relative_y < pos.height();
}

bool ComboBoxWidget::handleMouseWheel(int delta, int relative_x, int relative_y)
{
    (void)relative_x;
    (void)relative_y;
    if (!isEnabled() || isOpen() || m_items.empty() || delta == 0) {
        return false;
    }
    // Up (away from the user) is the previous item, like the Up key.
    const int count = static_cast<int>(m_items.size());
    const int from = m_selected < 0 ? (delta > 0 ? 0 : -1) : m_selected;
    choose(std::clamp(from - delta, 0, count - 1));
    return true;
}

void ComboBoxWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    const Rect& pos = getPos();
    m_screen_pos = Rect(parent_absolute_pos.x() + pos.x(), parent_absolute_pos.y() + pos.y(),
                        pos.width(), pos.height());
    m_screen_height = target.height();
    Widget::recursiveBlitTo(target, parent_absolute_pos);
}

void ComboBoxWidget::focus()
{
    if (s_focused_widget == this) {
        return;
    }
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
    s_focused_widget = this;
    m_focused = true;
    rebuildSurface();
}

void ComboBoxWidget::blur()
{
    if (isOpen()) {
        close(false);
    }
    m_focused = false;
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    rebuildSurface();
}

void ComboBoxWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

void ComboBoxWidget::open()
{
    if (isOpen() || m_items.empty()) {
        return;
    }
    if (s_open_widget) {
        s_open_widget->close(false);
    }
    s_open_widget = this;

    // Below the control, unless the list would run off the bottom of the
    // screen and fits above it.
    const int list_h = visibleRows() * itemHeight() + 2;
    m_list_above = m_screen_height > 0 &&
                   m_screen_pos.y() + m_screen_pos.height() + list_h > m_screen_height &&
                   m_screen_pos.y() - list_h >= 0;

    const int count = static_cast<int>(m_items.size());
    const int rows = visibleRows();
    if (m_scrollbar) {
        const int range = count - rows;
        m_scrollbar->setGeometry(Rect(getPos().width() - kScrollbarWidth, 0, kScrollbarWidth, list_h));
        m_scrollbar->setSteps(1.0 / range, static_cast<double>(rows) / range);
    }
    m_hot = m_selected >= 0 ? m_selected : 0;
    m_hot_ants = false;
    m_top = -1; // force setTop to sync the scrollbar
    setTop(std::clamp(m_hot - rows / 2, 0, std::max(0, count - rows)));
    rebuildList();
    rebuildSurface();
}

void ComboBoxWidget::close(bool commit)
{
    if (!isOpen()) {
        return;
    }
    const int hot = m_hot;
    s_open_widget = nullptr;
    m_tracking = false;
    m_list_pressed = false;
    m_drag_entered = false;
    // A scrollbar press still held loses its release with the list (nothing
    // routes events to it once closed): end it, freeing its mouse capture.
    if (m_scrollbar_pressed && m_scrollbar) {
        m_scrollbar->cancelGesture();
    }
    m_scrollbar_pressed = false;
    m_list_surface.reset();
    rebuildSurface();
    if (commit) {
        choose(hot);
    }
}

int ComboBoxWidget::visibleRows() const
{
    return std::min(static_cast<int>(m_items.size()), kMaxVisibleRows);
}

int ComboBoxWidget::itemHeight() const
{
    return std::max(12, (m_font ? m_font->lineHeight() : 14) + 2);
}

bool ComboBoxWidget::hasScrollbar() const
{
    return m_scrollbar != nullptr;
}

Rect ComboBoxWidget::listRect() const
{
    // The list's border shares the control's: its top line is the control's
    // bottom line (its bottom line the control's top, opened above).
    const Rect& pos = getPos();
    const int h = visibleRows() * itemHeight() + 2;
    return Rect(0, m_list_above ? -h + 1 : pos.height() - 1, pos.width(), h);
}

int ComboBoxWidget::listRowAt(int relative_x, int relative_y) const
{
    const Rect list = listRect();
    int right = list.x() + list.width() - 1; // exclusive of the border
    if (hasScrollbar()) {
        right = list.x() + list.width() - kScrollbarWidth;
    }
    if (relative_x < list.x() + 1 || relative_x >= right) {
        return -1;
    }
    const int y = relative_y - (list.y() + 1);
    if (y < 0 || y >= visibleRows() * itemHeight()) {
        return -1;
    }
    const int index = m_top + y / itemHeight();
    return index < static_cast<int>(m_items.size()) ? index : -1;
}

void ComboBoxWidget::setTop(int top)
{
    const int count = static_cast<int>(m_items.size());
    top = std::clamp(top, 0, std::max(0, count - visibleRows()));
    if (top == m_top) {
        return;
    }
    m_top = top;
    if (m_scrollbar) {
        const int range = count - visibleRows();
        // Notifies the scrollbar's callback, which finds m_top already set.
        m_scrollbar->setValue(range > 0 ? static_cast<double>(top) / range : 0.0);
    }
    rebuildList();
}

void ComboBoxWidget::setHot(int index)
{
    const int count = static_cast<int>(m_items.size());
    if (count == 0) {
        return;
    }
    index = std::clamp(index, 0, count - 1);
    const int rows = visibleRows();
    if (index < m_top) {
        setTop(index);
    } else if (index >= m_top + rows) {
        setTop(index - rows + 1);
    }
    if (index != m_hot) {
        m_hot = index;
        rebuildList();
    }
}

void ComboBoxWidget::rebuildSurface()
{
    const Rect& pos = getPos();
    const int w = pos.width();
    const int h = pos.height();
    auto surface = std::make_unique<Surface>(w, h, true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));
    surface->rectangle(0, 0, w - 1, h - 1, 0, 0, 0, 255);

    // The field, inside the border and left of the button, drawn into its own
    // surface so a long item is clipped there. Focused, it is the navy
    // highlight with the focus ants, 1px in from the border all round; not
    // while the list is open, when the highlight is the list's.
    const int field_w = std::max(1, w - kButtonWidth - 1);
    const int field_h = std::max(1, h - 2);
    Surface field(field_w, field_h, true);
    field.FillRect(field.MapRGBA(255, 255, 255, 255));
    const bool highlighted = m_focused && isEnabled() && !isOpen();
    if (highlighted && field_w > 3 && field_h > 3) {
        field.box(1, 1, field_w - 2, field_h - 2, 0, 0, 128, 255);
        drawFocusAnts(field, 1, 1, field_w - 2, field_h - 2, kNavy);
    }
    if (m_font && m_selected >= 0) {
        const uint8_t fg = highlighted ? 255 : (isEnabled() ? 0 : 128);
        auto text = highlighted
            ? m_font->RenderLCD(m_items[static_cast<size_t>(m_selected)], fg, fg, fg, 0, 0, 128)
            : m_font->RenderLCD(m_items[static_cast<size_t>(m_selected)], fg, fg, fg, 255, 255, 255);
        if (text) {
            const int ty = (field_h - text->height()) / 2;
            // Clip to the navy area so the text never covers the ants.
            Surface clip(std::max(1, field_w - 5), std::max(1, field_h - 4), true);
            clip.FillRect(highlighted ? clip.MapRGBA(0, 0, 128, 255) : clip.MapRGBA(255, 255, 255, 255));
            clip.Blit(*text, Rect(1, ty - 2, text->width(), text->height()));
            field.Blit(clip, Rect(2, 2, clip.width(), clip.height()));
        }
    }
    surface->Blit(field, Rect(1, 1, field_w, field_h));

    // The button: the divider and the right border are its outline.
    const bool pressed = m_tracking && isOpen();
    const Rect button(w - kButtonWidth, 0, kButtonWidth, h);
    drawComboButton(*surface, button, pressed);
    const int shift = pressed ? 1 : 0;
    drawDropGlyph(*surface, button.x() + kButtonWidth / 2 + shift,
                  (h - kGlyphHeight) / 2 + shift, isEnabled() ? 0 : 128);

    setSurface(std::move(surface));
}

void ComboBoxWidget::rebuildList()
{
    if (!isOpen()) {
        return;
    }
    const Rect list = listRect();
    auto surface = std::make_unique<Surface>(list.width(), list.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));
    surface->rectangle(0, 0, list.width() - 1, list.height() - 1, 0, 0, 0, 255);

    // Rows run between the borders, and stop at the scrollbar's left edge
    // (its own frame) when there is one.
    const int row_w = hasScrollbar() ? list.width() - kScrollbarWidth - 1 : list.width() - 2;
    const int item_h = itemHeight();
    for (int r = 0; r < visibleRows(); ++r) {
        const int index = m_top + r;
        if (index >= static_cast<int>(m_items.size())) {
            break;
        }
        const bool hot = index == m_hot;
        Surface row(std::max(1, row_w), item_h, true);
        if (hot) {
            row.FillRect(row.MapRGBA(0, 0, 128, 255));
        } else {
            row.FillRect(row.MapRGBA(255, 255, 255, 255));
        }
        if (m_font) {
            auto text = hot
                ? m_font->RenderLCD(m_items[static_cast<size_t>(index)], 255, 255, 255, 0, 0, 128)
                : m_font->RenderLCD(m_items[static_cast<size_t>(index)], 0, 0, 0, 255, 255, 255);
            if (text) {
                row.Blit(*text, Rect(2, (item_h - text->height()) / 2, text->width(), text->height()));
            }
        }
        if (hot && m_hot_ants) {
            drawFocusAnts(row, 0, 0, row_w - 1, item_h - 1, kNavy);
        }
        surface->Blit(row, Rect(1, 1 + r * item_h, row_w, item_h));
    }
    m_list_surface = std::move(surface);
}

void ComboBoxWidget::blitOpenList(Surface& target)
{
    ComboBoxWidget* w = s_open_widget;
    if (!w) {
        return;
    }
    // Drawing runs once a frame, so it is the drag-scroll clock: the list
    // keeps scrolling while a held pointer rests past its edge.
    w->autoScrollTick();
    if (!w->m_list_surface) {
        return;
    }
    const Rect list = w->listRect();
    const Rect at(w->m_screen_pos.x() + list.x(), w->m_screen_pos.y() + list.y(),
                  list.width(), list.height());
    target.Blit(*w->m_list_surface, at);
    if (w->m_scrollbar) {
        w->m_scrollbar->recursiveBlitTo(target, at);
    }
}

bool ComboBoxWidget::routeOpenListMouseDown(const SDL_MouseButtonEvent& event, int x, int y)
{
    ComboBoxWidget* w = s_open_widget;
    if (!w) {
        return false;
    }
    return w->listMouseDown(event, x - w->m_screen_pos.x(), y - w->m_screen_pos.y());
}

bool ComboBoxWidget::routeOpenListMouseMotion(const SDL_MouseMotionEvent& event, int x, int y)
{
    ComboBoxWidget* w = s_open_widget;
    if (!w) {
        return false;
    }
    w->listMouseMotion(event, x - w->m_screen_pos.x(), y - w->m_screen_pos.y());
    return true;
}

bool ComboBoxWidget::routeOpenListMouseUp(const SDL_MouseButtonEvent& event, int x, int y)
{
    ComboBoxWidget* w = s_open_widget;
    if (!w) {
        return false;
    }
    w->listMouseUp(event, x - w->m_screen_pos.x(), y - w->m_screen_pos.y());
    return true;
}

bool ComboBoxWidget::routeOpenListMouseWheel(int delta)
{
    ComboBoxWidget* w = s_open_widget;
    if (!w) {
        return false;
    }
    w->setTop(w->m_top - delta);
    return true;
}

bool ComboBoxWidget::listMouseDown(const SDL_MouseButtonEvent& event, int rx, int ry)
{
    const Rect list = listRect();
    if (!list.contains(rx, ry)) {
        // Anywhere else, the control's own button included: close, and eat
        // the click.
        close(false);
        return true;
    }
    if (hasScrollbar() && rx >= list.x() + list.width() - kScrollbarWidth) {
        // Only the left button works the scrollbar, and only its release
        // ends that (the scrollbar ignores the others).
        if (event.button == SDL_BUTTON_LEFT && !m_scrollbar_pressed) {
            m_scrollbar_pressed = true;
            m_scrollbar->handleMouseDown(event, rx - (list.x() + list.width() - kScrollbarWidth), ry - list.y());
        }
        return true;
    }
    if (event.button == SDL_BUTTON_LEFT) {
        // A press highlights; its release picks (see listMouseUp).
        const int row = listRowAt(rx, ry);
        if (row >= 0) {
            m_list_pressed = true;
            m_drag_entered = true;
            m_drag_y = ry;
            m_hot_ants = false;
            setHot(row);
            rebuildList();
        }
    }
    return true;
}

void ComboBoxWidget::listMouseMotion(const SDL_MouseMotionEvent& event, int rx, int ry)
{
    const Rect list = listRect();
    if (m_scrollbar_pressed) {
        m_scrollbar->handleMouseMotion(event, rx - (list.x() + list.width() - kScrollbarWidth), ry - list.y());
        return;
    }
    // The highlight follows the pointer over the rows, without the ants. A
    // held press follows it by row whatever the x; above or below the rows,
    // autoScrollTick takes over.
    const bool held = m_tracking || m_list_pressed;
    m_drag_y = ry;
    const int row = held ? listRowAtY(ry) : listRowAt(rx, ry);
    if (row >= 0 && (row != m_hot || m_hot_ants)) {
        if (held) {
            m_drag_entered = true;
        }
        m_hot_ants = false;
        setHot(row);
        rebuildList();
    }
}

void ComboBoxWidget::listMouseUp(const SDL_MouseButtonEvent& event, int rx, int ry)
{
    const Rect list = listRect();
    if (m_scrollbar_pressed) {
        if (event.button == SDL_BUTTON_LEFT) {
            m_scrollbar_pressed = false;
            m_scrollbar->handleMouseUp(event, rx - (list.x() + list.width() - kScrollbarWidth), ry - list.y());
        }
        return;
    }
    if (!m_tracking && !m_list_pressed) {
        return;
    }
    // The release of a held press picks the item under it, or, once the drag
    // has been over the list, the highlighted item wherever it ends. The
    // press that opened the list, released without reaching it, leaves the
    // list open for a click.
    const bool entered = m_drag_entered;
    m_tracking = false;
    m_list_pressed = false;
    m_drag_entered = false;
    const int row = listRowAt(rx, ry);
    if (row >= 0) {
        m_hot = row;
        close(true);
    } else if (entered) {
        close(true);
    } else {
        rebuildSurface();
    }
}

int ComboBoxWidget::listRowAtY(int relative_y) const
{
    const Rect list = listRect();
    const int y = relative_y - (list.y() + 1);
    if (y < 0 || y >= visibleRows() * itemHeight()) {
        return -1;
    }
    const int index = m_top + y / itemHeight();
    return index < static_cast<int>(m_items.size()) ? index : -1;
}

void ComboBoxWidget::autoScrollTick()
{
    if (!(m_tracking || m_list_pressed) || m_scrollbar_pressed) {
        return;
    }
    const Rect list = listRect();
    const int rows_top = list.y() + 1;
    const int rows_bottom = rows_top + visibleRows() * itemHeight();
    int step = 0;
    int distance = 0;
    if (m_drag_y < rows_top) {
        step = -1;
        distance = rows_top - m_drag_y;
    } else if (m_drag_y >= rows_bottom) {
        step = 1;
        distance = m_drag_y - rows_bottom + 1;
    }
    // The press that opened the list starts on the field, above (or below)
    // the rows: that isn't a drag off the list until it has been over it.
    if (step == 0 || (m_tracking && !m_drag_entered)) {
        return;
    }
    // Faster the further past the edge, like the list view's drag scroll.
    const Uint64 interval = static_cast<Uint64>(std::max(30, 150 - 3 * distance));
    const Uint64 now = SDL_GetTicks();
    if (now - m_last_autoscroll_ms < interval) {
        return;
    }
    m_last_autoscroll_ms = now;
    m_hot_ants = false;
    setHot(m_hot + step);
    rebuildList();
}

bool ComboBoxWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    ComboBoxWidget* w = s_focused_widget;
    if (!w || !w->isEnabled()) {
        return false;
    }
    const bool alt = (keysym.mod & SDL_KMOD_ALT) != 0;
    const int count = static_cast<int>(w->m_items.size());
    const int rows = std::max(1, w->visibleRows());

    // The next item after `from` (wrapping) whose text starts with `c`.
    auto match = [&](int from, char c) -> int {
        for (int i = 1; i <= count; ++i) {
            const int index = ((from < 0 ? -1 : from) + i) % count;
            const std::string text = w->m_items[static_cast<size_t>(index)].to8Bit(true);
            if (!text.empty() && std::tolower(static_cast<unsigned char>(text[0])) == c) {
                return index;
            }
        }
        return -1;
    };
    const bool letter = (keysym.sym >= SDLK_A && keysym.sym <= SDLK_Z) ||
                        (keysym.sym >= SDLK_0 && keysym.sym <= SDLK_9);

    if (w->isOpen()) {
        // A keyboard move in the open list changes the choice there and then
        // (the field shows it), and the highlight wears the focus ants.
        auto key_hot = [w](int index) {
            w->m_hot_ants = true;
            w->setHot(index);
            w->rebuildList();
            w->choose(w->m_hot);
        };
        switch (keysym.sym) {
            case SDLK_UP:
                if (alt) {
                    w->close(true);
                } else {
                    key_hot(w->m_hot - 1);
                }
                return true;
            case SDLK_DOWN:
                if (alt) {
                    w->close(true);
                } else {
                    key_hot(w->m_hot + 1);
                }
                return true;
            case SDLK_PAGEUP:
                key_hot(w->m_hot - (rows - 1));
                return true;
            case SDLK_PAGEDOWN:
                key_hot(w->m_hot + (rows - 1));
                return true;
            case SDLK_HOME:
                key_hot(0);
                return true;
            case SDLK_END:
                key_hot(count - 1);
                return true;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_F4:
                w->close(true);
                return true;
            case SDLK_ESCAPE:
                w->close(false);
                return true;
            default:
                break;
        }
        if (letter && !alt) {
            const int index = match(w->m_hot, static_cast<char>(keysym.sym));
            if (index >= 0) {
                key_hot(index);
            }
        }
        // Open, the list owns the keyboard: nothing reaches the global keys.
        return true;
    }

    if ((alt && keysym.sym == SDLK_DOWN) || keysym.sym == SDLK_F4) {
        w->open();
        w->m_hot_ants = true; // opened from the keyboard
        w->rebuildList();
        return true;
    }
    if (alt) {
        return false; // menu mnemonics
    }
    const int selected = w->m_selected;
    switch (keysym.sym) {
        case SDLK_UP:
        case SDLK_LEFT:
            w->choose(selected < 0 ? 0 : std::max(0, selected - 1));
            return true;
        case SDLK_DOWN:
        case SDLK_RIGHT:
            w->choose(std::min(count - 1, selected + 1));
            return true;
        case SDLK_PAGEUP:
            w->choose(std::max(0, selected - (rows - 1)));
            return true;
        case SDLK_PAGEDOWN:
            w->choose(std::min(count - 1, std::max(0, selected) + (rows - 1)));
            return true;
        case SDLK_HOME:
            w->choose(0);
            return true;
        case SDLK_END:
            w->choose(count - 1);
            return true;
        case SDLK_ESCAPE:
            // Drop focus rather than let Escape reach the quit key.
            w->blur();
            return true;
        // Enter goes on to the window's default button, Tab to traversal.
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_TAB:
            return false;
        default:
            break;
    }
    if (keysym.sym >= SDLK_F1 && keysym.sym <= SDLK_F12) {
        return false;
    }
    if (letter) {
        const int index = match(selected, static_cast<char>(keysym.sym));
        if (index >= 0) {
            w->choose(index);
        }
    }
    // Swallow the rest so the global shortcuts (Q quits, Space pauses, ...)
    // don't fire while the control has focus.
    return true;
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
