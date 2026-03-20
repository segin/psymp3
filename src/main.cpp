/*
 * main.cpp - contains main(), mostly.
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

#include "psymp3.h"

#ifdef HAVE_MP3
// RAII wrapper for libmpg123 initialization and cleanup.
// A single static instance of this class ensures that mpg123_init() is called
// once at program startup and mpg123_exit() is called once at program termination.
/**
 * @brief RAII wrapper that manages the lifetime of the libmpg123 library.
 *
 * The constructor calls `mpg123_init()` and the destructor calls `mpg123_exit()`,
 * ensuring the library is initialised exactly once before any MP3 decoding
 * takes place and cleaned up when the process exits.
 */
class Mpg123LifecycleManager {
public:
    /** @brief Initialises libmpg123. Throws `std::runtime_error` on failure. */
    Mpg123LifecycleManager() {
        if (mpg123_init() != MPG123_OK) {
            // Throwing an exception is a clear way to signal a fatal startup error.
            throw std::runtime_error("Failed to initialize libmpg123.");
        }
    }
    /** @brief Shuts down libmpg123 by calling `mpg123_exit()`. */
    ~Mpg123LifecycleManager() {
        mpg123_exit();
    }
};
#endif

// RAII wrapper for Windows Winsock initialization and cleanup
#ifdef _WIN32
/**
 * @brief RAII wrapper that manages Winsock initialisation and cleanup (Windows only).
 *
 * Calls `WSAStartup` in the constructor and `WSACleanup` in the destructor,
 * ensuring Winsock is active for the lifetime of the process.
 */
class WinsockLifecycleManager {
public:
    WinsockLifecycleManager() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("Failed to initialize Winsock.");
        }
    }
    ~WinsockLifecycleManager() {
        WSACleanup();
    }
};
static WinsockLifecycleManager winsock_manager;
#endif

#ifdef HAVE_MP3
// The global static instance that manages the library's lifetime.
// Its constructor is called before main(), and its destructor is called after main() exits.
static Mpg123LifecycleManager mpg123_manager;
#endif

/**
 * @brief Application entry point.
 *
 * Parses command-line options (FFT mode, scale, decay, debug channels, log
 * file, test mode, version, help), initialises the debug subsystem, collects
 * non-option file-path arguments, and then creates and runs the `Player`.
 *
 * Long options:
 *   - `--fft <mat-og|vibe-1|neomat-in|neomat-out>` – select FFT visualisation
 *   - `--scale <N>` – FFT scale factor
 *   - `--decay <F>` – FFT decay factor
 *   - `--debug <channels>` – comma-separated debug channels (or `all`)
 *   - `--logfile <path>` – redirect debug output to a file
 *   - `--test` – enable automated test mode
 *   - `--unattended-quit` – exit after the playlist finishes
 *   - `--no-mpris-errors` – suppress MPRIS error messages
 *   - `-v` / `--version` – print version info and exit
 *   - `-h` / `--help` – print usage and exit
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return 0 on normal exit, 1 on invalid option.
 */
int main(int argc, char *argv[]) {
    // --- Argument Parsing ---
    PlayerOptions options;
    std::string logfile;
    std::vector<std::string> debug_channels;
    bool should_run = true;

    static const struct option long_options[] = {
        {"fft", required_argument, 0, 0},
        {"scale", required_argument, 0, 0},
        {"decay", required_argument, 0, 0},
        {"test", no_argument, 0, 0},
        {"version", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {"debug", required_argument, 0, 0},
        {"logfile", required_argument, 0, 0},
        {"unattended-quit", no_argument, 0, 0},
        {"no-mpris-errors", no_argument, 0, 0},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "vh", long_options, &option_index)) != -1) {
        if (opt == 0) {
            std::string option_name = long_options[option_index].name;
            if (option_name == "fft") {
                if (strcmp(optarg, "mat-og") == 0) options.fft_mode = FFTMode::Original;
                else if (strcmp(optarg, "vibe-1") == 0) options.fft_mode = FFTMode::Optimized;
                else if (strcmp(optarg, "neomat-in") == 0) options.fft_mode = FFTMode::NeomatIn;
                else if (strcmp(optarg, "neomat-out") == 0) options.fft_mode = FFTMode::NeomatOut;
            } else if (option_name == "scale") {
                options.scalefactor = atoi(optarg);
            } else if (option_name == "decay") {
                options.decayfactor = atof(optarg);
            } else if (option_name == "test") {
                options.automated_test_mode = true;
            } else if (option_name == "debug") {
                if (strcmp(optarg, "all") == 0) {
                    debug_channels.push_back("all");
                } else {
                    // Split the comma-separated list of debug channels
                    std::string channels_str(optarg);
                    std::istringstream channels_stream(channels_str);
                    std::string channel;
                    while (std::getline(channels_stream, channel, ',')) {
                        debug_channels.push_back(channel);
                    }
                }
            } else if (option_name == "logfile") {
                logfile = optarg;
            } else if (option_name == "unattended-quit") {
                options.unattended_quit = true;
            } else if (option_name == "no-mpris-errors") {
                options.show_mpris_errors = false;
            }
        } else {
            switch (opt) {
                case 'v':
                    about_console();
                    should_run = false;
                    break;
                case 'h':
                    print_help();
                    should_run = false;
                    break;
                case '?': // Invalid option
                    return 1;
            }
        }
    }

    Debug::init(logfile, debug_channels);

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

    Debug::shutdown();
    return 0;
}
