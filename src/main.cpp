/*
 * main.cpp - contains main(), mostly.
 * This file is part of PsyMP3.
 * Copyright © 2011-2020 Kirn Gill <segin2005@gmail.com>
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

#ifdef _WIN32
// Convert wide-character string to UTF-8
std::string WideCharToUTF8(const wchar_t* wideStr) {
    int wideStrLen = wcslen(wideStr);
    int utf8StrLen = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, NULL, 0, NULL, NULL);
    if (utf8StrLen == 0) {
        return ""; // Conversion failed
    }
    std::vector<char> buffer(utf8StrLen + 1); // +1 for null terminator
    WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, buffer.data(), utf8StrLen, NULL, NULL);
    return std::string(buffer.data());
}

// Parse command line arguments on Windows
std::vector<std::string> ParseCommandLine(int argc, char *argv[]) {
    std::vector<std::string> args;

    LPWSTR* wideArgv;
    int wideArgc;

    wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);
    if (wideArgv == NULL) {
        std::cerr << "Failed to parse command line" << std::endl;
        exit(1);
    }

    // Convert wide-character argv to UTF-8
    for (int i = 0; i < wideArgc; ++i) {
        args.push_back(WideCharToUTF8(wideArgv[i]));
    }

    LocalFree(wideArgv); // Free memory allocated by CommandLineToArgvW

    return args;
}

#else
// Parse command line arguments on Linux
std::vector<std::string> ParseCommandLine(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
    }
    return args;
}
#endif


int main(int argc, char *argv[])
{
    Player PsyMP3;
    PsyMP3.Run(ParseCommandLine(argc, argv));
    return 0;
}
