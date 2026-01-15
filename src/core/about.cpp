/*
 * about.cpp - Print about info to either dialog box or console
 * This file is part of PsyMP3.
 * Copyright © 2011-2026 Kirn Gill <segin2005@gmail.com>
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

#ifndef FINAL_BUILD
#include "psymp3.h"
#endif // !FINAL_BUILD

namespace PsyMP3 {
namespace Core {


static char _about_message[] = "This is PsyMP3 version " PSYMP3_VERSION ".\n"\
            "\n"
            "Copyright © 2009-2026 Kirn Gill II <segin2005@gmail.com>\n"
            "Copyright © 2010-2026 Mattis Michel <sic_zer0@hotmail.com>\n"
            "Copyright (c) 2009-2025 Rajesh Rajan <seanawake@gmail.com>\n"
            "Font \"Droid Sans Fallback\" is Copyright © 2006-2026 Google, Inc.\n"
            "\n"
            "PsyMP3 is free software. You may redistribute and/or modify it under\n"
            "the terms of the ISC License <https://opensource.org/licenses/ISC>\n"
            "\n"
            "Permission to use, copy, modify, and/or distribute this software for any\n"
            "purpose with or without fee is hereby granted, provided that the above\n"
            "copyright notice and this permission notice appear in all copies.\n"
            "\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n"
            "WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n"
            "MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n"
            "ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n"
            "WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
            "ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n"
            "OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
            "\n"
            "Written by " PSYMP3_MAINTAINER "\n";

/**
 * @brief Prints the application's about information to the standard console output.
 * 
 * This function is typically used for command-line invocations or on non-GUI platforms
 * to display version, copyright, and licensing details.
 */
void about_console()
{
    std::cout << _about_message << std::endl;
}

/**
 * @brief Prints GNU-style help information to the standard console output.
 * 
 * This function displays usage information, command-line options, and available
 * debug channels in the standard GNU help format.
 */
void print_help()
{
    std::cout << "Usage: psymp3 [OPTION]... [FILE]...\n";
    std::cout << "A multimedia player application supporting various audio formats.\n\n";
    
    std::cout << "Options:\n";
    std::cout << "  -h, --help              display this help and exit\n";
    std::cout << "  -v, --version           output version information and exit\n";
    std::cout << "      --fft=MODE          set FFT mode (mat-og, vibe-1, neomat-in, neomat-out)\n";
    std::cout << "      --scale=FACTOR      set scale factor for visualization\n";
    std::cout << "      --decay=FACTOR      set decay factor for visualization\n";
    std::cout << "      --test              enable automated test mode\n";
    std::cout << "      --debug=CHANNELS    enable debug output for specified channels\n";
    std::cout << "                          (comma-separated list or 'all')\n";
    std::cout << "      --logfile=FILE      write debug output to specified file\n";
    std::cout << "      --unattended-quit   quit automatically when playback ends\n";
    std::cout << "      --no-mpris-errors   disable on-screen notifications for MPRIS errors\n\n";
    
    std::cout << "Available debug channels:\n";
    std::cout << "  HTTPIOHandler, audio, chunk, codec, compliance, demux, demuxer,\n";
    std::cout << "  display, error, flac, flac_benchmark, flac_codec, flac_rfc_validator,\n";
    std::cout << "  font, http, io, iso, iso_compliance, lastfm, loader, lyrics, memory,\n";
    std::cout << "  mp3, mpris, ogg, opus, opus_codec, performance, player, playlist,\n";
    std::cout << "  plugin, raii, resource, spectrum, stream, streaming, system, test,\n";
    std::cout << "  timer, vorbis, widget\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  psymp3 song.mp3                    Play a single file\n";
    std::cout << "  psymp3 --debug=flac,demux file.flac\n";
    std::cout << "                                      Play with FLAC and demux debugging\n";
    std::cout << "  psymp3 --debug=all --logfile=debug.log\n";
    std::cout << "                                      Enable all debug channels with log file\n\n";
    
    std::cout << "Report bugs to: segin2005@gmail.com\n";
    std::cout << "PsyMP3 home page: <https://github.com/segin/psymp3>\n";
}

#if defined(_WIN32)
/**
 * @brief Displays the application's about information in a native Windows message box.
 * 
 * This function handles both Unicode and non-Unicode builds. For Unicode, it converts
 * the UTF-8 about message to a wide string before displaying it. For non-Unicode,
 * it displays the message directly.
 */
void about_windows() {
#ifdef UNICODE
    // Use modern C++ vector for buffer management instead of malloc/free.
    // First, determine the required buffer size for the wide string.
    int required_size = MultiByteToWideChar(CP_UTF8, 0, _about_message, -1, NULL, 0);
    if (required_size > 0) {
        std::vector<wchar_t> wide_buffer(required_size);
        // Now, perform the actual conversion.
        MultiByteToWideChar(CP_UTF8, 0, _about_message, -1, wide_buffer.data(), required_size);
        MessageBoxW(System::getHwnd(), wide_buffer.data(), L"PsyMP3", MB_OK);
    }
#else
    MessageBoxA(System::getHwnd(), _about_message, "PsyMP3", MB_OK);
#endif
}
#endif // _WIN32

} // namespace Core
} // namespace PsyMP3
