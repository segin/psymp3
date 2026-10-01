/*
 * RadioButtonWidget.h - Windows 3.1 style radio (option) button
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#ifndef RADIOBUTTONWIDGET_H
#define RADIOBUTTONWIDGET_H

// No direct includes - all includes should be in psymp3.h

namespace PsyMP3 {
namespace Widget {
namespace UI {

// One of a group of mutually exclusive options: a ring, with a dot when
// selected, and a label. Selecting one (a click, or Space, on release)
// deselects the others in its group; a selected button can't be unselected
// by the user. Held down, the ring thickens, as the check box's border does.
// It takes keyboard focus (click or Tab), shown as the dotted rectangle round
// the label, and the arrow keys move the selection (and the focus) through
// its group, as in Windows.
class RadioButtonWidget : public Widget {
public:
    RadioButtonWidget(int width, int height, Font* font, const TagLib::String& text = "");
    ~RadioButtonWidget() override;

    // Put these buttons in one group, in this order (the arrow keys' order).
    static void makeGroup(const std::vector<RadioButtonWidget*>& buttons);

    bool isSelected() const { return m_selected; }
    // Select this button (deselecting the rest of its group) without firing
    // any callback.
    void setSelected();
    // Fired on the button the user selects.
    void setOnSelect(std::function<void()> cb) { m_on_select = std::move(cb); }

    bool handleMouseDown(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseUp(const SDL_MouseButtonEvent& event, int relative_x, int relative_y) override;
    bool handleMouseMotion(const SDL_MouseMotionEvent& event, int relative_x, int relative_y) override;

    // Keyboard focus, following CheckboxWidget's pattern.
    static RadioButtonWidget* focusedWidget() { return s_focused_widget; }
    static void clearFocusedWidget();
    static bool handleFocusedKeyPress(const SDL_keysym& keysym);
    static bool handleFocusedKeyUp(const SDL_keysym& keysym);
    void takeFocus(); // Tab-traversal entry point

private:
    struct Group {
        std::vector<RadioButtonWidget*> members;
    };

    void rebuildSurface();
    void blur();
    void choose(); // the user's selection: select, then fire m_on_select

    Font* m_font;
    TagLib::String m_text;
    bool m_selected = false;
    bool m_pressed = false;       // held down, by the mouse or Space
    bool m_key_pressed = false;   // Space is held
    bool m_mouse_held = false;    // a left press holds the mouse capture
    std::function<void()> m_on_select;
    std::shared_ptr<Group> m_group;

    static RadioButtonWidget* s_focused_widget;
};

} // namespace UI
} // namespace Widget
} // namespace PsyMP3

#endif // RADIOBUTTONWIDGET_H
