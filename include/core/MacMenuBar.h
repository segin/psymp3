/*
 * MacMenuBar.h - PsyMP3's menus in the macOS menu bar (PsyMP3::Core)
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

#ifndef PSYMP3_CORE_MACMENUBAR_H
#define PSYMP3_CORE_MACMENUBAR_H

/*
 * Plain C++ on purpose, like MacNowPlaying.h: the Objective-C++ that builds
 * the NSMenus stays in src/core/MacMenuBar.mm, built on macOS only
 * (PSYMP3_MAC_MENUBAR). The player hands over a copy of its menus' shape;
 * what each item does, and whether it is enabled or checked, it answers by
 * id through the callbacks.
 */
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Core {

/**
 * @brief PsyMP3's own menus (File, Playback, Settings, Help) in the macOS
 *        menu bar, after the application menu macOS gives every app.
 */
class MacMenuBar {
public:
    /** @brief One menu entry, as the menu bar shows it. */
    struct Item {
        std::string label;           ///< with PsyMP3's '&' mnemonic markers, which are dropped
        std::vector<Item> children;  ///< non-empty: a submenu
        bool separator = false;      ///< a divider
        int id = -1;                 ///< a leaf's id for the callbacks; -1 for the rest
        /// The key equivalent, as macOS writes it: modifiers ⌃⌥⇧⌘ then the key
        /// ("⌘O", "⌥⌘E", "⌘←"); "" for none.
        std::string key_equivalent;
    };

    /** @brief A top-level menu: its title and its items. */
    struct Menu {
        std::string title;
        std::vector<Item> items;
    };

    /** @brief What the menu bar asks of the player, by item id. */
    struct Callbacks {
        void (*activate)(int id);  ///< the item was picked
        bool (*enabled)(int id);   ///< asked each time a menu opens
        bool (*checked)(int id);   ///< likewise: draws a check mark
    };

    /**
     * @brief Put @p menus in the menu bar after the application menu,
     *        replacing any put there before. Main thread only.
     * @param menus     The menus, in order.
     * @param callbacks How the items reach the player.
     */
    static void install(const std::vector<Menu>& menus, Callbacks callbacks);
};

} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_MACMENUBAR_H
