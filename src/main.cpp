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
#include <getopt.h>

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

int main(int argc, char *argv[]) {
    // --- Argument Parsing ---
    // Arguments are parsed here in main, before the Player object is created.
    // This separates command-line interface logic from application logic.
    PlayerOptions options;
    bool should_run = true;

    static const struct option long_options[] = {
        {"fft", required_argument, 0, 'f'},
        {"scale", required_argument, 0, 's'},
        {"decay", required_argument, 0, 'd'},
        {"test", no_argument, 0, 't'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    // Use getopt_long directly on the original argc and argv.
    while ((opt = getopt_long(argc, argv, "f:s:d:tv", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'f':
                if (strcmp(optarg, "mat-og") == 0) options.fft_mode = FFTMode::Original;
                else if (strcmp(optarg, "vibe-1") == 0) options.fft_mode = FFTMode::Optimized;
                else if (strcmp(optarg, "neomat-in") == 0) options.fft_mode = FFTMode::NeomatIn;
                else if (strcmp(optarg, "neomat-out") == 0) options.fft_mode = FFTMode::NeomatOut;
                break;
            case 's':
                options.scalefactor = atoi(optarg);
                break;
            case 'd':
                options.decayfactor = atof(optarg);
                break;
            case 't':
                options.automated_test_mode = true;
                break;
            case 'v':
                Player::about_console();
                should_run = false;
                break;
            case '?': // Invalid option
                return 1; // getopt_long already prints an error message.
        }
    }

    // Collect non-option arguments as file paths.
    if (optind < argc) {
        for (int i = optind; i < argc; ++i) {
            options.files.push_back(argv[i]);
        }
    }

    if (should_run) {
        Player psymp3_player;
        psymp3_player.Run(options);
    }

    return 0;
}
