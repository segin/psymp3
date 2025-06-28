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

// RAII wrapper for libmpg123 initialization and cleanup.
// A single static instance of this class ensures that mpg123_init() is called
// once at program startup and mpg123_exit() is called once at program termination.
class Mpg123LifecycleManager {
public:
    Mpg123LifecycleManager() {
        if (mpg123_init() != MPG123_OK) {
            // Throwing an exception is a clear way to signal a fatal startup error.
            throw std::runtime_error("Failed to initialize libmpg123.");
        }
    }
    ~Mpg123LifecycleManager() {
        mpg123_exit();
    }
};

// The global static instance that manages the library's lifetime.
// Its constructor is called before main(), and its destructor is called after main() exits.
static Mpg123LifecycleManager mpg123_manager;

#ifdef _WIN32

std::string WideCharToUTF8(const wchar_t* wideStr) {
    auto wideStrLen = wcslen(wideStr);
    auto utf8StrLen = WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, nullptr, 0, nullptr, nullptr);
    if (utf8StrLen == 0) 
        return "";
    std::vector<char> buffer(utf8StrLen + 1); 
    WideCharToMultiByte(CP_UTF8, 0, wideStr, wideStrLen, buffer.data(), utf8StrLen, nullptr, nullptr);
    return std::string(buffer.data());
}

std::vector<std::string> ParseCommandLine(int, char *[]) {
    std::vector<std::string> args;
    int wideArgc;
    auto wideArgv = CommandLineToArgvW(GetCommandLineW(), &wideArgc);;
    if (wideArgv == nullptr) {
        std::cerr << "Failed to parse command line" << std::endl;
        exit(1);
    }
    for (int i = 0; i < wideArgc; ++i) 
        args.push_back(WideCharToUTF8(wideArgv[i]));
    LocalFree(wideArgv);
    return args;
}

#else
// Parse command line arguments on Linux
std::vector<std::string> ParseCommandLine(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (auto i = 0; i < argc; ++i)
        args.push_back(argv[i]);
    return args;
}
#endif

int main(int argc, char *argv[]) {
    Player PsyMP3;
    PsyMP3.Run(ParseCommandLine(argc, argv));
    return 0;
}
