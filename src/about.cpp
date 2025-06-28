/*
 * about.cpp - Print about info to either dialog box or console
 * This file is part of PsyMP3.
 * Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
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

#include "psymp3.h"

static char _about_message[] = "This is PsyMP3 version " PSYMP3_VERSION ".\n"\
            "\n"
            "Copyright © 2009-2025 Kirn Gill II <segin2005@gmail.com>\n"
            "Copyright © 2010-2025 Mattis Michel <sic_zer0@hotmail.com>\n"
            "Copyright (c) 2009-2025 Rajesh Rajan <seanawake@gmail.com>\n"
            "Font \"Droid Sans Fallback\" is Copyright © 2006-2025 Google, Inc.\n"
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

void about_console()
{
    std::cout << _about_message << std::endl;
}

#if defined(_WIN32)
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