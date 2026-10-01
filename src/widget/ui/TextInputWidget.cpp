/*
 * TextInputWidget.cpp - Windows 3.1 style single-line text input widget
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

TextInputWidget* TextInputWidget::s_focused_widget = nullptr;

namespace {

// The Windows 3.1 edit field is flat: a 1px black border round white. Text
// is drawn into the area inside it, so a long line is clipped there instead
// of painting over the border's right edge.
constexpr int kFrameInset = 1;
// Left edge of the text (its pen position), in widget coordinates.
constexpr int kTextLeft = 3;
// Windows' default caret blink time (GetCaretBlinkTime).
constexpr Uint64 kBlinkMs = 530;

std::string narrowText(const TagLib::String& text)
{
    return text.to8Bit(true);
}

bool isContinuationByte(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

// Codepoint boundaries in a UTF-8 string.
size_t previousBoundaryIn(const std::string& s, size_t index)
{
    if (index == 0 || s.empty()) {
        return 0;
    }
    size_t i = std::min(index, s.size()) - 1;
    while (i > 0 && isContinuationByte(s[i])) {
        --i;
    }
    return i;
}

size_t nextBoundaryIn(const std::string& s, size_t index)
{
    if (index >= s.size()) {
        return s.size();
    }
    size_t i = index + 1;
    while (i < s.size() && isContinuationByte(s[i])) {
        ++i;
    }
    return i;
}

// Keep printable ASCII (0x20-0x7E) and every UTF-8 multi-byte byte (>= 0x80);
// drop only C0 controls and DEL. Dropping CR/LF also collapses multi-line
// clipboard content into the single line this control edits. Invalid UTF-8
// is replaced with U+FFFD first: TagLib empties a string it can't decode,
// which would wipe the whole field on paste.
std::string filterPrintable(const std::string& input)
{
    const std::string valid = UTF8Util::isValid(input) ? input : UTF8Util::repair(input);
    std::string filtered;
    filtered.reserve(valid.size());
    for (unsigned char c : valid) {
        if (c >= 0x20 && c != 0x7F) {
            filtered.push_back(static_cast<char>(c));
        }
    }
    return filtered;
}

} // namespace

TextInputWidget::TextInputWidget(int width, int height, Font* font, const TagLib::String& text)
    : Widget()
    , m_font(font)
    , m_text(text)
{
    m_caret = m_anchor = narrowText(text).size();
    setPos(Rect(0, 0, width, height));
    rebuildSurface();
}

TextInputWidget::~TextInputWidget()
{
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    // Its context menu's items point at it.
    ContextMenuWidget::forget(this);
    // A field closed under the pointer (Enter in a dialog, say) would
    // otherwise leave the I-beam showing: only window frames reset the cursor
    // as the pointer moves.
    if (m_hovered || m_pressed) {
        showDefaultCursor();
    }
}

void TextInputWidget::showTextCursor()
{
    // Created once and never destroyed: SDL_Quit frees every cursor, and
    // destroying one after that is a double free (see ~WindowFrameWidget).
    static SDL_Cursor* s_text_cursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    if (s_text_cursor && SDL_GetCursor() != s_text_cursor) {
        SDL_SetCursor(s_text_cursor);
    }
}

void TextInputWidget::showDefaultCursor()
{
    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        return;
    }
    SDL_Cursor* arrow = SDL_GetDefaultCursor();
    if (arrow && SDL_GetCursor() != arrow) {
        SDL_SetCursor(arrow);
    }
}

bool TextInputWidget::handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (!isEnabled() || !handlesPoint(relative_x, relative_y)) {
        return false;
    }
    if (event.button == SDL_BUTTON_RIGHT) {
        // Focus without disturbing the selection, and offer the edit commands.
        if (!m_focused) {
            focus(false);
        }
        openContextMenu(relative_x, relative_y);
        return true;
    }
    if (event.button != SDL_BUTTON_LEFT) {
        return false;
    }

    // Shift+click extends the selection of a field that already has focus.
    const bool extend = m_focused && (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    if (!m_focused) {
        focus(false);
    }
    const size_t index = indexAtX(relative_x);
    m_word_select = event.clicks >= 2;
    if (m_word_select) {
        selectWordAt(index);
    } else {
        moveCaret(index, extend);
    }
    // Capture so the drag keeps selecting when the pointer leaves the field.
    m_pressed = true;
    captureMouse();
    return true;
}

bool TextInputWidget::handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y)
{
    if (event.button != SDL_BUTTON_LEFT || !m_pressed) {
        return false;
    }

    m_pressed = false;
    m_word_select = false;
    releaseMouse();
    m_hovered = handlesPoint(relative_x, relative_y);
    // The I-beam stayed through the drag; a release outside the field ends it.
    if (!m_hovered) {
        showDefaultCursor();
    }
    rebuildSurface();
    return true;
}

bool TextInputWidget::handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y)
{
    (void)event;

    const bool hovered = handlesPoint(relative_x, relative_y);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        rebuildSurface();
    }
    // Set on every motion, not just on entry: a window frame puts the arrow
    // back on each motion before passing it on to the widgets inside it. A
    // drag that selects keeps the I-beam wherever the pointer goes.
    // Leaving is handleMouseLeave's job: resetting here for a pointer that is
    // elsewhere would fight the frame's resize cursors.
    if ((hovered && isEnabled()) || m_pressed) {
        showTextCursor();
    }
    // Dragging selects from where the button went down. A double-click's
    // word stays selected: the small movements that follow it would
    // otherwise shrink it to part of the word.
    if (m_pressed && !m_word_select) {
        const size_t index = indexAtX(relative_x);
        if (index != m_caret) {
            moveCaret(index, true);
        }
    }

    return hovered || m_pressed;
}

void TextInputWidget::handleMouseLeave()
{
    // Motion events stop arriving the moment the pointer leaves; this is the
    // only notification, so the hover ring must clear here.
    if (m_hovered) {
        m_hovered = false;
        rebuildSurface();
        invalidate();
    }
    if (!m_pressed) {
        showDefaultCursor();
    }
    Widget::handleMouseLeave();
}

void TextInputWidget::recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos)
{
    // Redraw only when the blink phase has flipped since the last rebuild.
    if (m_focused && caretPhaseVisible() != m_caret_drawn) {
        rebuildSurface();
        invalidate();
    }
    Widget::recursiveBlitTo(target, parent_absolute_pos);
}

void TextInputWidget::setText(const TagLib::String& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    m_caret = m_anchor = narrowText(m_text).size();
    m_visible_start = 0;
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
}

void TextInputWidget::setPlaceholder(const TagLib::String& placeholder)
{
    if (m_placeholder == placeholder) {
        return;
    }

    m_placeholder = placeholder;
    rebuildSurface();
}

void TextInputWidget::clearFocusedWidget()
{
    if (s_focused_widget) {
        s_focused_widget->blur();
    }
}

bool TextInputWidget::handleFocusedKeyPress(const SDL_keysym& keysym)
{
    if (!s_focused_widget) {
        return false;
    }

    TextInputWidget& widget = *s_focused_widget;
    const bool ctrl = (keysym.mod & SDL_KMOD_CTRL) != 0;
    const bool shift = (keysym.mod & SDL_KMOD_SHIFT) != 0;
    const bool alt = (keysym.mod & SDL_KMOD_ALT) != 0;

    // Clipboard and select-all chords. Control chords never arrive as
    // SDL_EVENT_TEXT_INPUT, so these must be handled at the keysym level.
    if (ctrl && !alt) {
        switch (keysym.sym) {
            case SDLK_A:
                widget.selectAll();
                return true;
            case SDLK_C:
            case SDLK_INSERT:
                return widget.copyToClipboard();
            case SDLK_X:
                return widget.cutToClipboard();
            case SDLK_V:
                return widget.pasteFromClipboard();
            default:
                break;
        }
    }
    // The older CUA clipboard keys, which Windows edit controls still honour.
    if (shift && !ctrl && !alt) {
        if (keysym.sym == SDLK_INSERT) {
            return widget.pasteFromClipboard();
        }
        if (keysym.sym == SDLK_DELETE) {
            return widget.cutToClipboard();
        }
    }

    const size_t length = narrowText(widget.m_text).size();
    switch (keysym.sym) {
        case SDLK_BACKSPACE:
            return widget.eraseBeforeCaret();
        case SDLK_DELETE:
            return widget.eraseAtCaret();
        // Shift extends the selection; without it, a plain arrow collapses a
        // selection to its near edge, and Ctrl moves a word at a time.
        case SDLK_LEFT: {
            size_t target;
            if (ctrl) {
                target = widget.previousWordStart(widget.m_caret);
            } else if (widget.hasSelection() && !shift) {
                target = widget.selectionStart();
            } else {
                target = widget.previousBoundary(widget.m_caret);
            }
            widget.moveCaret(target, shift);
            return true;
        }
        case SDLK_RIGHT: {
            size_t target;
            if (ctrl) {
                target = widget.nextWordStart(widget.m_caret);
            } else if (widget.hasSelection() && !shift) {
                target = widget.selectionEnd();
            } else {
                target = widget.nextBoundary(widget.m_caret);
            }
            widget.moveCaret(target, shift);
            return true;
        }
        case SDLK_HOME:
            widget.moveCaret(0, shift);
            return true;
        case SDLK_END:
            widget.moveCaret(length, shift);
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Called through a copy: the callback may destroy the widget.
            if (widget.m_on_submit) {
                auto on_submit = widget.m_on_submit;
                on_submit();
                return true;
            }
            // Without a handler of its own, Enter is the dialog's: its
            // default button fires, as from a Windows edit control.
            return false;
        case SDLK_ESCAPE: {
            // Blur rather than fall through: the global handler treats a
            // leaked Escape as quit-the-program.
            auto on_cancel = widget.m_on_cancel;
            widget.blur();
            if (on_cancel) {
                on_cancel();
            }
            return true;
        }
        default:
            break;
    }

    // Swallow everything else so the global shortcuts (Q quits, Space pauses,
    // N skips, ...) cannot fire while the user is typing; the printable keys
    // arrive separately as SDL_EVENT_TEXT_INPUT. Only two families escape a
    // focused box: Alt chords, so the menu-bar mnemonics stay reachable, and
    // the function keys, which carry no text meaning.
    if (alt) {
        return false;
    }
    if (keysym.sym >= SDLK_F1 && keysym.sym <= SDLK_F12) {
        return false;
    }
    return true;
}

bool TextInputWidget::handleFocusedTextInput(const char* text)
{
    if (!s_focused_widget || !text || text[0] == '\0') {
        return false;
    }

    TextInputWidget& widget = *s_focused_widget;

    // SDL delivers SDL_EVENT_TEXT_INPUT as UTF-8. Insert the whole filtered
    // sequence in one step so a multi-byte codepoint's bytes are never split
    // across separate edits.
    const std::string filtered = filterPrintable(text);
    if (filtered.empty()) {
        return false;
    }
    return widget.insertString(filtered);
}

void TextInputWidget::focus(bool select_all)
{
    if (s_focused_widget && s_focused_widget != this) {
        s_focused_widget->blur();
    }

    s_focused_widget = this;
    m_focused = true;
    if (select_all) {
        m_anchor = 0;
        m_caret = narrowText(m_text).size();
    }
    resetBlink();
    rebuildSurface();
}

void TextInputWidget::blur()
{
    if (m_pressed) {
        releaseMouse();
    }
    m_pressed = false;
    m_word_select = false;
    m_focused = false;
    if (s_focused_widget == this) {
        s_focused_widget = nullptr;
    }
    rebuildSurface();
}

bool TextInputWidget::handlesPoint(int relative_x, int relative_y) const
{
    const Rect& pos = getPos();
    return relative_x >= 0 && relative_x < pos.width() &&
           relative_y >= 0 && relative_y < pos.height();
}

void TextInputWidget::moveCaret(size_t index, bool extend)
{
    m_caret = std::min(index, narrowText(m_text).size());
    if (!extend) {
        m_anchor = m_caret;
    }
    resetBlink();
    rebuildSurface();
}

void TextInputWidget::selectAll()
{
    m_anchor = 0;
    m_caret = narrowText(m_text).size();
    resetBlink();
    rebuildSurface();
}

void TextInputWidget::selectWordAt(size_t index)
{
    const std::string text = narrowText(m_text);
    if (text.empty()) {
        moveCaret(0, false);
        return;
    }
    // The run under the pointer: the character after `index`, or the last one
    // when the click lands past the end. A run of spaces selects as a run too.
    size_t probe = std::min(index, text.size() - 1);
    const bool on_space = text[probe] == ' ';
    size_t start = probe;
    while (start > 0 && (text[start - 1] == ' ') == on_space) {
        --start;
    }
    size_t end = probe;
    while (end < text.size() && (text[end] == ' ') == on_space) {
        ++end;
    }
    m_anchor = start;
    m_caret = end;
    resetBlink();
    rebuildSurface();
}

size_t TextInputWidget::previousBoundary(size_t index) const
{
    return previousBoundaryIn(narrowText(m_text), index);
}

size_t TextInputWidget::nextBoundary(size_t index) const
{
    return nextBoundaryIn(narrowText(m_text), index);
}

size_t TextInputWidget::previousWordStart(size_t index) const
{
    // Spaces are ASCII, so every stop is a codepoint boundary.
    const std::string text = narrowText(m_text);
    size_t i = std::min(index, text.size());
    while (i > 0 && text[i - 1] == ' ') {
        --i;
    }
    while (i > 0 && text[i - 1] != ' ') {
        --i;
    }
    return i;
}

size_t TextInputWidget::nextWordStart(size_t index) const
{
    const std::string text = narrowText(m_text);
    size_t i = std::min(index, text.size());
    while (i < text.size() && text[i] != ' ') {
        ++i;
    }
    while (i < text.size() && text[i] == ' ') {
        ++i;
    }
    return i;
}

size_t TextInputWidget::indexAtX(int x) const
{
    const std::string display = displayText();
    const size_t start = std::min(m_visible_start, display.size());
    const int offset = x - kTextLeft;
    // Left of the text while dragging: step one character out of view, so
    // the view scrolls back a character per motion event.
    if (offset < 0 && start > 0) {
        return fromDisplayIndex(previousBoundaryIn(display, start));
    }
    // Nearest boundary: past the middle of a character counts as after it.
    int left = 0;
    size_t pos = start;
    while (pos < display.size()) {
        const size_t next = nextBoundaryIn(display, pos);
        const int width = displayWidth(display, pos, next);
        if (offset < left + width / 2) {
            return fromDisplayIndex(pos);
        }
        left += width;
        pos = next;
    }
    return fromDisplayIndex(display.size());
}

bool TextInputWidget::insertString(const std::string& utf8)
{
    if (utf8.empty()) {
        return false;
    }
    std::string text = narrowText(m_text);
    const size_t start = std::min(selectionStart(), text.size());
    const size_t end = std::min(selectionEnd(), text.size());
    text.replace(start, end - start, utf8);
    commitEdit(text, start + utf8.size());
    return true;
}

bool TextInputWidget::eraseBeforeCaret()
{
    if (hasSelection()) {
        deleteSelection();
        return true;
    }
    std::string text = narrowText(m_text);
    const size_t caret = std::min(m_caret, text.size());
    if (caret == 0) {
        return true;
    }
    // Erase the whole UTF-8 codepoint before the caret (its lead byte plus any
    // continuation bytes), not just a single byte.
    const size_t start = previousBoundaryIn(text, caret);
    text.erase(start, caret - start);
    commitEdit(text, start);
    return true;
}

bool TextInputWidget::eraseAtCaret()
{
    if (hasSelection()) {
        deleteSelection();
        return true;
    }
    std::string text = narrowText(m_text);
    const size_t caret = std::min(m_caret, text.size());
    if (caret >= text.size()) {
        return true;
    }
    // Erase the whole UTF-8 codepoint at the caret.
    const size_t end = nextBoundaryIn(text, caret);
    text.erase(caret, end - caret);
    commitEdit(text, caret);
    return true;
}

void TextInputWidget::deleteSelection()
{
    if (!hasSelection()) {
        return;
    }
    std::string text = narrowText(m_text);
    const size_t start = std::min(selectionStart(), text.size());
    const size_t end = std::min(selectionEnd(), text.size());
    text.erase(start, end - start);
    commitEdit(text, start);
}

bool TextInputWidget::copyToClipboard()
{
    // The chord is handled either way; a password field never gives its text up.
    if (m_password_mode || !hasSelection()) {
        return true;
    }
    const std::string text = narrowText(m_text);
    const size_t start = std::min(selectionStart(), text.size());
    const size_t end = std::min(selectionEnd(), text.size());
    SDL_SetClipboardText(text.substr(start, end - start).c_str());
    return true;
}

bool TextInputWidget::cutToClipboard()
{
    if (m_password_mode || !hasSelection()) {
        return true;
    }
    copyToClipboard();
    deleteSelection();
    return true;
}

void TextInputWidget::openContextMenu(int relative_x, int relative_y)
{
    const bool can_copy = hasSelection() && !m_password_mode;
    std::vector<MenuItem> items;
    items.push_back(MenuItem::command("C&ut", [this] { cutToClipboard(); }, can_copy, "Ctrl+X"));
    items.push_back(MenuItem::command("&Copy", [this] { copyToClipboard(); }, can_copy, "Ctrl+C"));
    items.push_back(MenuItem::command("&Paste", [this] { pasteFromClipboard(); }, SDL_HasClipboardText(),
                                      "Ctrl+V"));
    items.push_back(MenuItem::sep());
    items.push_back(MenuItem::command("Select &All", [this] { selectAll(); }, !m_text.isEmpty(), "Ctrl+A"));
    const Rect me = ContextMenuWidget::screenRectOf(this);
    ContextMenuWidget::popUp(m_font, std::move(items), me.x() + relative_x, me.y() + relative_y,
                             ContextMenuWidget::screenBounds(), this);
}

bool TextInputWidget::pasteFromClipboard()
{
    char* clip = SDL_GetClipboardText();
    if (!clip) {
        return true; // the chord is handled either way
    }
    const std::string filtered = filterPrintable(clip);
    SDL_free(clip);
    if (!filtered.empty()) {
        insertString(filtered);
    }
    return true;
}

void TextInputWidget::commitEdit(const std::string& utf8, size_t caret)
{
    m_text = TagLib::String(utf8, TagLib::String::UTF8);
    // Clamped against the text as stored, not as given: should the
    // conversion ever drop bytes, the caret must still lie within it.
    m_caret = m_anchor = std::min(caret, narrowText(m_text).size());
    resetBlink();
    rebuildSurface();
    if (m_on_change) {
        m_on_change(m_text);
    }
}

void TextInputWidget::setPasswordMode(bool on)
{
    if (m_password_mode == on) {
        return;
    }
    m_password_mode = on;
    rebuildSurface();
}

std::string TextInputWidget::displayText() const
{
    std::string text = narrowText(m_text);
    if (!m_password_mode) {
        return text;
    }
    // One '*' per codepoint: count the lead bytes.
    size_t codepoints = 0;
    for (char c : text) {
        if (!isContinuationByte(c)) {
            ++codepoints;
        }
    }
    return std::string(codepoints, '*');
}

size_t TextInputWidget::toDisplayIndex(size_t index) const
{
    if (!m_password_mode) {
        return index;
    }
    // In the masked text each codepoint is one byte, so the offset is the
    // number of codepoints before `index`.
    const std::string text = narrowText(m_text);
    size_t count = 0;
    for (size_t i = 0; i < std::min(index, text.size()); ++i) {
        if (!isContinuationByte(text[i])) {
            ++count;
        }
    }
    return count;
}

size_t TextInputWidget::fromDisplayIndex(size_t display_index) const
{
    if (!m_password_mode) {
        return display_index;
    }
    // The byte offset of the display_index'th codepoint.
    const std::string text = narrowText(m_text);
    size_t pos = 0;
    for (size_t n = 0; n < display_index && pos < text.size(); ++n) {
        pos = nextBoundaryIn(text, pos);
    }
    return pos;
}

int TextInputWidget::displayWidth(const std::string& display_text, size_t from, size_t to) const
{
    if (!m_font || to <= from) {
        return 0;
    }
    return m_font->measureWidth(display_text.substr(from, to - from));
}

void TextInputWidget::resetBlink()
{
    m_blink_epoch = SDL_GetTicks();
}

bool TextInputWidget::caretPhaseVisible() const
{
    return ((SDL_GetTicks() - m_blink_epoch) / kBlinkMs) % 2 == 0;
}

void TextInputWidget::rebuildSurface()
{
    const Rect& pos = getPos();
    auto surface = std::make_unique<Surface>(pos.width(), pos.height(), true);
    surface->FillRect(surface->MapRGBA(255, 255, 255, 255));
    surface->rectangle(0, 0, pos.width() - 1, pos.height() - 1, 0, 0, 0, 255);

    // Everything inside the frame is drawn into its own surface, which clips
    // it; coordinates below are relative to that area.
    const int area_w = std::max(1, pos.width() - 2 * kFrameInset);
    const int area_h = std::max(1, pos.height() - 2 * kFrameInset);
    Surface area(area_w, area_h, true);
    area.FillRect(area.MapRGBA(255, 255, 255, 255));

    const int text_x = kTextLeft - kFrameInset;
    const int text_width = std::max(1, pos.width() - 2 * kTextLeft);
    const int line_h = m_font ? m_font->lineHeight() : area_h;
    // Centred in the field (the caret spans the same line).
    const int text_y = std::max(0, (pos.height() - line_h) / 2 - kFrameInset);

    const std::string display = displayText();
    const size_t caret = std::min(toDisplayIndex(m_caret), display.size());

    // Scroll the view to keep the caret in it: back to the caret if it is
    // left of the view, forward until the text up to it fits, and back again
    // while the text from an earlier start still fits (after a deletion, say),
    // so the field never shows blank space the text could fill. Each step
    // lands on a codepoint boundary.
    size_t& start = m_visible_start;
    start = std::min(start, display.size());
    while (start > 0 && start < display.size() && isContinuationByte(display[start])) {
        --start;
    }
    if (caret < start) {
        start = caret;
    }
    while (start < caret && displayWidth(display, start, caret) + 1 > text_width) {
        start = nextBoundaryIn(display, start);
    }
    while (start > 0) {
        const size_t earlier = previousBoundaryIn(display, start);
        if (displayWidth(display, earlier, display.size()) + 1 > text_width) {
            break;
        }
        start = earlier;
    }

    // Draw the visible text as up to three runs: before, inside and after the
    // selection, the selection in white on the Windows 3.1 highlight navy. The
    // runs are measured with the advances they are drawn with, so each starts
    // exactly where the one before it ended.
    auto draw_run = [&](size_t from, size_t to, bool selected) {
        if (!m_font || to <= from) {
            return;
        }
        const int x = text_x + displayWidth(display, start, from);
        const TagLib::String run(display.substr(from, to - from), TagLib::String::UTF8);
        std::unique_ptr<Surface> glyphs;
        if (selected) {
            const int w = displayWidth(display, from, to);
            area.box(x, text_y, x + w - 1, text_y + line_h - 1, 0, 0, 128, 255);
            glyphs = m_font->RenderLCD(run, 255, 255, 255, 0, 0, 128);
        } else {
            // RenderLCD pre-blends subpixel glyphs against an opaque
            // background; the field is solid white, so blend against white.
            glyphs = m_font->RenderLCD(run, 0, 0, 0, 255, 255, 255);
        }
        if (glyphs) {
            area.Blit(*glyphs, Rect(x, text_y, glyphs->width(), glyphs->height()));
        }
    };

    const bool show_selection = m_focused && hasSelection();
    if (show_selection) {
        const size_t sel_from = std::clamp(toDisplayIndex(selectionStart()), start, display.size());
        const size_t sel_to = std::clamp(toDisplayIndex(selectionEnd()), start, display.size());
        draw_run(start, sel_from, false);
        draw_run(sel_from, sel_to, true);
        draw_run(sel_to, display.size(), false);
    } else {
        draw_run(start, display.size(), false);
    }

    if (display.empty() && !m_placeholder.isEmpty() && !m_focused && m_font) {
        auto placeholder = m_font->RenderLCD(m_placeholder, 128, 128, 128, 255, 255, 255);
        if (placeholder) {
            area.Blit(*placeholder, Rect(text_x, text_y, placeholder->width(), placeholder->height()));
        }
    }

    // The caret: one pixel wide at the boundary before the next character,
    // the height of the text line. White where it sits on the selection's
    // first column (the caret is at the selection's left end), black elsewhere.
    m_caret_drawn = m_focused && caretPhaseVisible();
    if (m_caret_drawn) {
        const int caret_x = std::min(text_x + displayWidth(display, start, caret), area_w - 1);
        const bool on_selection = show_selection && caret == toDisplayIndex(selectionStart());
        const uint8_t shade = on_selection ? 255 : 0;
        area.vline(caret_x, text_y, std::min(text_y + line_h, area_h) - 1, shade, shade, shade, 255);
    }

    surface->Blit(area, Rect(kFrameInset, kFrameInset, area_w, area_h));

    setSurface(std::move(surface));
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
