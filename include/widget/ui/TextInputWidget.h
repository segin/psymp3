/*
 * TextInputWidget.h - Windows 3.1 style single-line text input widget
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef TEXTINPUTWIDGET_H
#define TEXTINPUTWIDGET_H

namespace PsyMP3 {
namespace Widget {
namespace UI {

// A single-line edit control, keyed and moused like the Windows edit control:
// a blinking caret, a selection (Shift+arrows/Home/End, Ctrl+A, mouse drag,
// Shift+click, double-click for a word), Ctrl+Left/Right word moves, and the
// clipboard (Ctrl+C/X/V and Ctrl+Insert, Shift+Delete, Shift+Insert, and a
// right-click menu of Cut, Copy, Paste and Select All). Typing or pasting
// replaces the selection. The selection shows only while focused.
class TextInputWidget : public Widget {
public:
    TextInputWidget(int width, int height, Font* font, const TagLib::String& text = "");
    ~TextInputWidget() override;

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;
    void handleMouseLeave() override;
    // Rendering runs once a frame; it doubles as the caret's blink clock.
    void recursiveBlitTo(Surface& target, const Rect& parent_absolute_pos) override;

    void setText(const TagLib::String& text);
    const TagLib::String& getText() const { return m_text; }
    void setPlaceholder(const TagLib::String& placeholder);
    // Password echo: render one '*' per typed codepoint (the Windows 3.1
    // masked-edit convention) while the real text stays in m_text. Copy and
    // cut are refused in this mode, as Windows refuses them.
    void setPasswordMode(bool on);
    void setOnChange(std::function<void(const TagLib::String&)> on_change) { m_on_change = std::move(on_change); }
    // Enter / Escape while focused. Escape still drops focus first. The
    // callbacks may close the window holding this widget, so neither touches
    // the widget after calling them.
    void setOnSubmit(std::function<void()> on_submit) { m_on_submit = std::move(on_submit); }
    void setOnCancel(std::function<void()> on_cancel) { m_on_cancel = std::move(on_cancel); }

    static void clearFocusedWidget();
    static bool handleFocusedKeyPress(const SDL_keysym& keysym);
    static bool handleFocusedTextInput(const char* text);
    static TextInputWidget* focusedWidget() { return s_focused_widget; }
    // Tab-traversal entry point: focus with the whole text selected, as
    // tabbing into a Windows edit control does.
    void takeFocus() { focus(true); }

private:
    void focus(bool select_all);
    void blur();
    void rebuildSurface();
    bool handlesPoint(int relative_x, int relative_y) const;

    // Caret and selection, as byte offsets into the UTF-8 text, always on a
    // codepoint boundary. The selection runs between m_anchor and m_caret;
    // they are equal when nothing is selected.
    bool hasSelection() const { return m_anchor != m_caret; }
    size_t selectionStart() const { return std::min(m_anchor, m_caret); }
    size_t selectionEnd() const { return std::max(m_anchor, m_caret); }
    // Put the caret at `index`, extending the selection from the anchor or
    // (extend == false) collapsing it there.
    void moveCaret(size_t index, bool extend);
    void selectAll();
    void selectWordAt(size_t index);
    // Byte offset of the codepoint boundary before / after `index`, and of the
    // start of the previous / next word (words are separated by spaces, as in
    // the Windows edit control).
    size_t previousBoundary(size_t index) const;
    size_t nextBoundary(size_t index) const;
    size_t previousWordStart(size_t index) const;
    size_t nextWordStart(size_t index) const;
    // The text offset nearest a point `x` pixels into the widget.
    size_t indexAtX(int x) const;

    // Edits. Each replaces the selection (if any), then notifies m_on_change.
    bool insertString(const std::string& utf8);
    bool eraseBeforeCaret();
    bool eraseAtCaret();
    void deleteSelection();
    bool copyToClipboard();
    bool cutToClipboard();
    bool pasteFromClipboard();
    // Right-click: Cut, Copy, Paste, Select All, at a point in the field.
    void openContextMenu(int relative_x, int relative_y);
    // Store `utf8` as the text with the caret (and anchor) at `caret`, redraw,
    // and notify m_on_change.
    void commitEdit(const std::string& utf8, size_t caret);

    // The text as displayed ('*' per codepoint in password mode), and the
    // conversions between offsets into it and into the real text.
    std::string displayText() const;
    size_t toDisplayIndex(size_t index) const;
    size_t fromDisplayIndex(size_t display_index) const;
    // Pixel width of display_text's bytes [from, to).
    int displayWidth(const std::string& display_text, size_t from, size_t to) const;

    // Restart the blink with the caret shown (after any caret move or edit).
    void resetBlink();
    bool caretPhaseVisible() const;

    // The I-beam pointer over the field, and the arrow back when it leaves.
    static void showTextCursor();
    static void showDefaultCursor();

    Font* m_font;
    TagLib::String m_text;
    TagLib::String m_placeholder;
    std::function<void(const TagLib::String&)> m_on_change;
    std::function<void()> m_on_submit;
    std::function<void()> m_on_cancel;
    bool m_password_mode = false;
    size_t m_caret = 0;
    size_t m_anchor = 0;
    // First displayed byte of the display text; the view scrolls to keep the
    // caret inside it. Kept between redraws so the mouse maps onto what's shown.
    size_t m_visible_start = 0;
    Uint64 m_blink_epoch = 0;
    bool m_caret_drawn = false;   // whether the last rebuild drew the caret
    bool m_pressed = false;       // left button down in the field (selecting)
    bool m_word_select = false;   // the press was a double-click: keep the word
    bool m_hovered = false;
    bool m_focused = false;

    static TextInputWidget* s_focused_widget;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // TEXTINPUTWIDGET_H
