/*
 * Shortcut.h - the modifier PsyMP3's shortcuts take (PsyMP3::Core)
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

#ifndef PSYMP3_CORE_SHORTCUT_H
#define PSYMP3_CORE_SHORTCUT_H

/*
 * What is Ctrl+ elsewhere is ⌘ on macOS: open, save, copy, cut, paste,
 * select all. The few Ctrl shortcuts that would clash with macOS's own
 * (Ctrl+Up/Down moving playlist rows, Ctrl+F4 closing a window) keep
 * SDL_KMOD_CTRL and their "Ctrl+" hints, and do not use these.
 */
#include <string>

namespace PsyMP3 {
namespace Core {

#ifdef __APPLE__
/** @brief The modifier PsyMP3's shortcuts take: Command on macOS. */
inline constexpr SDL_Keymod kShortcutModifier = SDL_KMOD_GUI;
#else
/** @brief The modifier PsyMP3's shortcuts take: Ctrl. */
inline constexpr SDL_Keymod kShortcutModifier = SDL_KMOD_CTRL;
#endif

/**
 * @brief Whether the shortcut modifier is held.
 * @param mod The key event's modifiers.
 * @return true if it is.
 */
inline bool shortcutHeld(SDL_Keymod mod)
{
    return (mod & kShortcutModifier) != 0;
}

/**
 * @brief A shortcut's hint for the menus: "Ctrl+O", or "⌘O" on macOS.
 * @param key The key, as the hint shows it ("O").
 * @return The hint.
 */
inline std::string shortcutHint(const char* key)
{
#ifdef __APPLE__
    return std::string("⌘") + key;
#else
    return std::string("Ctrl+") + key;
#endif
}

} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_SHORTCUT_H
