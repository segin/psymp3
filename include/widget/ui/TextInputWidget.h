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

class TextInputWidget : public Widget {
public:
    TextInputWidget(int width, int height, Font* font, const TagLib::String& text = "");
    ~TextInputWidget() override;

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;

    void setText(const TagLib::String& text);
    const TagLib::String& getText() const { return m_text; }
    void setPlaceholder(const TagLib::String& placeholder);
    void setOnChange(std::function<void(const TagLib::String&)> on_change) { m_on_change = std::move(on_change); }

    static void clearFocusedWidget();
    static bool handleFocusedKeyPress(const SDL_keysym& keysym);
    static bool handleFocusedTextInput(const char* text);

private:
    void focus();
    void blur();
    void rebuildSurface();
    bool handlesPoint(int relative_x, int relative_y) const;
    bool insertCharacter(char c);
    bool insertString(const std::string& utf8);
    bool eraseBeforeCaret();
    bool eraseAtCaret();

    Font* m_font;
    TagLib::String m_text;
    TagLib::String m_placeholder;
    std::function<void(const TagLib::String&)> m_on_change;
    size_t m_caret_index;
    bool m_pressed;
    bool m_hovered;
    bool m_focused;

    static TextInputWidget* s_focused_widget;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // TEXTINPUTWIDGET_H
