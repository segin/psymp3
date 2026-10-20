/*
 * MacMenuBar.mm - PsyMP3's menus in the macOS menu bar (PsyMP3::Core)
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that
 * the above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * The menus become NSMenus under the application menu SDL makes (About,
 * Hide, Quit). Every leaf has one target, which hands its id to the
 * player's callbacks: picked, and -- each time its menu opens, through
 * validateMenuItem: -- whether it is enabled and checked, so the bar always
 * shows what the in-window menus would. Built with ARC, on macOS only.
 */

#import <AppKit/AppKit.h>

#include "core/MacMenuBar.h"

using PsyMP3::Core::MacMenuBar;

namespace {

/** @brief The player's callbacks; all null until install(). */
MacMenuBar::Callbacks g_callbacks = {nullptr, nullptr, nullptr};

/** @brief The menus install() added to the main menu, to replace them next time. */
NSMutableArray<NSMenuItem*>* g_installed = nil;

/**
 * @brief A label without PsyMP3's '&' mnemonic markers ("&&" stays "&").
 * @param label The label.
 * @return It, as an NSString.
 */
NSString* menuTitle(const std::string& label)
{
    std::string out;
    for (size_t i = 0; i < label.size(); ++i) {
        if (label[i] == '&') {
            if (i + 1 < label.size() && label[i + 1] == '&') {
                out += '&';
                ++i;
            }
            continue;
        }
        out += label[i];
    }
    NSString* s = [[NSString alloc] initWithBytes:out.data() length:out.size() encoding:NSUTF8StringEncoding];
    return s ? s : @"";
}

} // namespace

/** @brief The target of every PsyMP3 menu item: forwards picks and asks for state. */
@interface PsyMP3MenuTarget : NSObject <NSMenuItemValidation>
@end

@implementation PsyMP3MenuTarget

/**
 * @brief An item was picked: tell the player its id.
 * @param sender The item.
 */
- (void)pick:(NSMenuItem*)sender
{
    if (g_callbacks.activate) g_callbacks.activate(static_cast<int>(sender.tag));
}

/**
 * @brief Before a menu shows: each item's check mark, and whether it can be picked.
 * @param item The item.
 * @return Whether it is enabled.
 */
- (BOOL)validateMenuItem:(NSMenuItem*)item
{
    const int id = static_cast<int>(item.tag);
    if (id < 0) return YES;
    if (g_callbacks.checked) {
        item.state = g_callbacks.checked(id) ? NSControlStateValueOn : NSControlStateValueOff;
    }
    return g_callbacks.enabled ? (g_callbacks.enabled(id) ? YES : NO) : YES;
}

@end

namespace {

/** @brief The one target all items share; kept alive here (menu targets are weak). */
PsyMP3MenuTarget* g_target = nil;

/**
 * @brief Fill @p menu with @p items, submenus and all.
 * @param menu  The NSMenu.
 * @param items PsyMP3's items.
 */
void fillMenu(NSMenu* menu, const std::vector<MacMenuBar::Item>& items)
{
    for (const MacMenuBar::Item& item : items) {
        if (item.separator) {
            [menu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        NSMenuItem* entry = [[NSMenuItem alloc] initWithTitle:menuTitle(item.label) action:nil keyEquivalent:@""];
        if (!item.children.empty()) {
            NSMenu* sub = [[NSMenu alloc] initWithTitle:entry.title];
            sub.autoenablesItems = YES;
            fillMenu(sub, item.children);
            entry.submenu = sub;
        } else {
            entry.target = g_target;
            entry.action = @selector(pick:);
            entry.tag = item.id;
        }
        [menu addItem:entry];
    }
}

} // namespace

namespace PsyMP3 {
namespace Core {

/**
 * @brief Add the menus after the application menu, removing the last ones.
 * @param menus     The menus.
 * @param callbacks The player's callbacks.
 */
void MacMenuBar::install(const std::vector<Menu>& menus, Callbacks callbacks)
{
    g_callbacks = callbacks;
    if (!g_target) g_target = [[PsyMP3MenuTarget alloc] init];

    NSMenu* bar = [NSApp mainMenu];
    if (!bar) {
        bar = [[NSMenu alloc] initWithTitle:@""];
        [NSApp setMainMenu:bar];
    }
    for (NSMenuItem* old in g_installed) [bar removeItem:old];
    g_installed = [NSMutableArray array];

    // After the application menu (SDL's, with About, Hide and Quit), or first.
    NSInteger at = bar.numberOfItems > 0 ? 1 : 0;
    for (const Menu& m : menus) {
        NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:menuTitle(m.title) action:nil keyEquivalent:@""];
        NSMenu* menu = [[NSMenu alloc] initWithTitle:top.title];
        menu.autoenablesItems = YES;
        fillMenu(menu, m.items);
        top.submenu = menu;
        [bar insertItem:top atIndex:at++];
        [g_installed addObject:top];
    }
}

} // namespace Core
} // namespace PsyMP3
