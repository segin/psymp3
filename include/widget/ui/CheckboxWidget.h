/*
 * CheckboxWidget.h - Windows 3.1 style checkbox widget
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef CHECKBOXWIDGET_H
#define CHECKBOXWIDGET_H

namespace PsyMP3 {
namespace Widget {
namespace UI {

class CheckboxWidget : public Widget {
public:
    CheckboxWidget(int width, int height, Font* font, const TagLib::String& text = "", bool checked = false);
    ~CheckboxWidget() override = default;

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;

    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }

    void setText(const TagLib::String& text);
    void setOnToggle(std::function<void(bool)> callback) { m_on_toggle = callback; }

private:
    void rebuildSurface();

    Font* m_font;
    TagLib::String m_text;
    bool m_checked;
    bool m_pressed;
    bool m_hovered;
    std::function<void(bool)> m_on_toggle;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // CHECKBOXWIDGET_H
