/*
 * ContextMenuWidget.cpp - Right-click (context) menus
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

namespace {
// The one open context menu.
struct OpenContextMenu {
    std::unique_ptr<MenuPopup> popup;
    const void* owner = nullptr;
    std::function<void()> on_close;
    Rect pass_through;
    // Where the pointer was when the menu opened, and whether a release may
    // pick yet: only once the pointer has left that spot or pressed in the
    // menu. Clamped off the screen's bottom or right edge, the menu can open
    // with an item under the pointer, and the release of the right-click that
    // opened it must not pick that item.
    int open_x = 0;
    int open_y = 0;
    bool armed = false;
};
OpenContextMenu s_context_menu;
Rect s_context_screen(0, 0, 640, 404);
} // namespace

ContextMenuWidget::ContextMenuWidget(int width, int height, Core::Font* font)
    : Widget()
    , m_font(font)
{
    setPos(Rect(0, 0, width, height));
    setMouseTransparent(true); // a handle: the open menu takes its events itself
}

ContextMenuWidget::~ContextMenuWidget()
{
    forget(this);
}

void ContextMenuWidget::setEntries(std::vector<MenuItem> entries)
{
    m_entries = std::move(entries);
}

void ContextMenuWidget::resize(int width, int height)
{
    const Rect pos = getPos();
    setPos(Rect(pos.x(), pos.y(), width, height));
}

void ContextMenuWidget::openAt(int x, int y)
{
    const Rect me = screenRectOf(this);
    Rect pass_through;
    if (m_pass_through.width() > 0 && m_pass_through.height() > 0) {
        pass_through = Rect(me.x() + m_pass_through.x(), me.y() + m_pass_through.y(),
                            m_pass_through.width(), m_pass_through.height());
    }
    popUp(m_font, m_entries, me.x() + x, me.y() + y, screenBounds(), this,
          [this] {
              if (m_on_close) {
                  auto on_close = m_on_close; // the callback may replace it
                  on_close();
              }
          },
          pass_through);
}

void ContextMenuWidget::close()
{
    closeFor(this);
}

bool ContextMenuWidget::isOpen() const
{
    return isOpenFor(this);
}

// The handle draws nothing and takes no clicks: while open, the menu gets
// events from the Player before any widget does.
bool ContextMenuWidget::handleMouseDown(const SDL_MouseButtonEvent&, int, int) { return false; }
bool ContextMenuWidget::handleMouseMotion(const SDL_MouseMotionEvent&, int, int) { return false; }
bool ContextMenuWidget::handleMouseUp(const SDL_MouseButtonEvent&, int, int) { return false; }

void ContextMenuWidget::popUp(Core::Font* font, std::vector<MenuItem> items, int screen_x, int screen_y,
                              const Rect& screen_bounds, const void* owner,
                              std::function<void()> on_close, const Rect& pass_through)
{
    closeOpenMenu();
    if (items.empty()) {
        return;
    }
    s_context_menu.popup = std::make_unique<MenuPopup>(font);
    s_context_menu.popup->open(std::move(items), screen_x, screen_y, screen_bounds);
    s_context_menu.owner = owner;
    s_context_menu.on_close = std::move(on_close);
    s_context_menu.pass_through = pass_through;
    s_context_menu.open_x = screen_x;
    s_context_menu.open_y = screen_y;
    s_context_menu.armed = false;
}

bool ContextMenuWidget::isOpenFor(const void* owner)
{
    return s_context_menu.popup && s_context_menu.owner == owner && owner != nullptr;
}

bool ContextMenuWidget::anyOpen()
{
    return s_context_menu.popup != nullptr;
}

bool ContextMenuWidget::openMenuContains(int x, int y)
{
    return s_context_menu.popup && s_context_menu.popup->contains(x, y);
}

void ContextMenuWidget::closeFor(const void* owner)
{
    if (isOpenFor(owner)) {
        closeOpenMenu();
    }
}

void ContextMenuWidget::forget(const void* owner)
{
    if (isOpenFor(owner)) {
        s_context_menu = OpenContextMenu();
    }
}

void ContextMenuWidget::closeOpenMenu()
{
    if (!s_context_menu.popup) {
        return;
    }
    auto on_close = std::move(s_context_menu.on_close);
    s_context_menu = OpenContextMenu();
    if (on_close) {
        on_close();
    }
}

bool ContextMenuWidget::routeMouseDown(const SDL_MouseButtonEvent& event, int x, int y)
{
    (void)event;
    MenuPopup* popup = s_context_menu.popup.get();
    if (!popup) {
        return false;
    }
    if (s_context_menu.pass_through.contains(x, y)) {
        return false; // its owner's to handle, menu still open
    }
    if (popup->contains(x, y)) {
        // Any button highlights; the release picks.
        s_context_menu.armed = true;
        popup->mouseDown(x, y);
        return true;
    }
    // Anywhere else: close, and eat the click.
    closeOpenMenu();
    return true;
}

bool ContextMenuWidget::routeMouseMotion(const SDL_MouseMotionEvent& event, int x, int y)
{
    (void)event;
    MenuPopup* popup = s_context_menu.popup.get();
    if (!popup) {
        return false;
    }
    if (x != s_context_menu.open_x || y != s_context_menu.open_y) {
        s_context_menu.armed = true;
    }
    popup->mouseMotion(x, y);
    return true;
}

bool ContextMenuWidget::routeMouseUp(const SDL_MouseButtonEvent& event, int x, int y)
{
    (void)event;
    MenuPopup* popup = s_context_menu.popup.get();
    if (!popup) {
        return false;
    }
    if (s_context_menu.pass_through.contains(x, y)) {
        return false;
    }
    // A release on an item picks it, with either button: a right-button
    // press that opened the menu can be dragged onto an item and released.
    // The release of that press without moving picks nothing, even where the
    // menu was pushed back on-screen with an item under the pointer.
    if (!s_context_menu.armed) {
        return true;
    }
    std::function<void()> picked;
    popup->mouseUp(x, y, picked);
    if (picked) {
        closeOpenMenu();
        picked();
    }
    return true;
}

bool ContextMenuWidget::routeKey(const SDL_keysym& keysym)
{
    MenuPopup* popup = s_context_menu.popup.get();
    if (!popup) {
        return false;
    }
    std::function<void()> picked;
    switch (popup->key(keysym, picked)) {
        case MenuPopup::KeyResult::Handled:
        case MenuPopup::KeyResult::Previous: // no neighbouring menus
        case MenuPopup::KeyResult::Next:
            break;
        case MenuPopup::KeyResult::Close:
            closeOpenMenu();
            break;
        case MenuPopup::KeyResult::Picked:
            closeOpenMenu();
            if (picked) {
                picked();
            }
            break;
    }
    return true; // the open menu owns the keyboard
}

void ContextMenuWidget::blitOpenMenu(Surface& target)
{
    if (s_context_menu.popup) {
        s_context_menu.popup->draw(target);
    }
}

void ContextMenuWidget::setScreenSize(int width, int height)
{
    s_context_screen = Rect(0, 0, width, height);
}

Rect ContextMenuWidget::screenBounds()
{
    return s_context_screen;
}

Rect ContextMenuWidget::screenRectOf(const Widget* widget)
{
    const Rect pos = widget->getPos();
    int x = pos.x();
    int y = pos.y();
    for (const Widget* p = widget->getParent(); p; p = p->getParent()) {
        x += p->getPos().x();
        y += p->getPos().y();
    }
    return Rect(x, y, pos.width(), pos.height());
}

} // namespace UI
} // namespace Widget
} // namespace PsyMP3
