/*
 * MacNowPlaying.h - macOS Now Playing and media keys (PsyMP3::Core)
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

#ifndef PSYMP3_CORE_MACNOWPLAYING_H
#define PSYMP3_CORE_MACNOWPLAYING_H

/*
 * Plain C++ on purpose: the Objective-C++ that talks to the MediaPlayer
 * framework stays in src/core/MacNowPlaying.mm, built on macOS only
 * (PSYMP3_MAC_NOWPLAYING), so nothing else sees Cocoa.
 */
#include <cstdint>
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Core {

/**
 * @brief macOS's equivalent of MPRIS: the system's Now Playing (Control
 *        Centre, the menu bar widget, the lock screen) and the media keys,
 *        AirPods and Touch Bar controls that go with it.
 *
 * Every call is made on the main thread. Commands from the system arrive
 * there too, through the callback given to start().
 */
class MacNowPlaying {
public:
    /** @brief What the system can ask the player to do. */
    enum class Command : intptr_t {
        Play = 1,
        Pause,
        TogglePlayPause,
        Stop,
        Next,
        Previous,
        SeekTo, ///< to the position given with it, in milliseconds
    };

    /** @brief Where playback stands, as the system shows it. */
    enum class State {
        Stopped,
        Playing,
        Paused,
    };

    /** @brief Receives a command, with a position in milliseconds for SeekTo. */
    using CommandHandler = void (*)(Command command, uint64_t position_ms);

    /**
     * @brief Become the system's now-playing app: register for its commands.
     * @param handler Called on the main thread for each command.
     */
    static void start(CommandHandler handler);

    /** @brief Give the commands up and clear what the system shows. */
    static void stop();

    /**
     * @brief Show what plays now.
     * @param artist      The artist, UTF-8, or "".
     * @param title       The title, UTF-8, or "".
     * @param album       The album, UTF-8, or "".
     * @param length_ms   Its length; 0 for internet radio, which is shown as
     *                    live and cannot be seeked.
     * @param position_ms Where it is now.
     * @param state       Playing, paused or stopped.
     * @param artwork     The cover's image bytes (JPEG, PNG...), or empty.
     */
    static void update(const std::string& artist, const std::string& title, const std::string& album,
                       uint64_t length_ms, uint64_t position_ms, State state,
                       const std::vector<uint8_t>& artwork);
};

} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_MACNOWPLAYING_H
