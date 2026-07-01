/*
 * FileDialog.h - Native "open file" chooser (PsyMP3::Core)
 * This file is part of PsyMP3.
 * Copyright © 2025 Kirn Gill <segin2005@gmail.com>
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

#ifndef PSYMP3_CORE_FILEDIALOG_H
#define PSYMP3_CORE_FILEDIALOG_H

// Deliberately self-contained: this header is included by the umbrella
// (psymp3.h) and by FileDialog.cpp, which also pulls in a GUI toolkit (Qt/GTK).
// Keeping the public surface to std::string/std::vector avoids dragging SDL,
// X11 or TagLib symbols into the same translation unit as Qt/GTK, where they
// are prone to macro/type collisions.
#include <string>
#include <vector>

namespace PsyMP3 {
namespace Core {

/**
 * @brief Native "open file" dialog, backed by whichever toolkit was selected at
 *        build time (Qt 3-6, GTK+ 2-4, or Win32). Compiled out entirely when no
 *        backend is available (HAVE_FILEDIALOG undefined).
 */
class FileDialog {
public:
    /**
     * @brief Show a modal open-file dialog. MUST be called from the main thread.
     *
     * @param allow_multiple  Permit selecting more than one file.
     * @param title           Dialog title (UTF-8).
     * @param extensions      Lower-case extensions WITHOUT a leading dot
     *                        (e.g. {"mp3","flac"}) used to build the type filter.
     *                        Empty means "no filter / all files".
     * @return Selected paths (UTF-8). Empty if the user cancelled, or if no
     *         toolkit backend is compiled in.
     */
    static std::vector<std::string> openFiles(bool allow_multiple,
                                              const std::string& title,
                                              const std::vector<std::string>& extensions);
};

} // namespace Core
} // namespace PsyMP3

#endif // PSYMP3_CORE_FILEDIALOG_H
