/*
 * psymp3.h - main include for all other source files.
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

#ifndef __PSYMP3_H__
#define __PSYMP3_H__

// defines
#define PSYMP3_VERSION "2-CURRENT"
#define PSYMP3_MAINTAINER "Kirn Gill II <segin2005@gmail.com>"

enum {
    RUN_GUI_ITERATION = 0xfe0f,
    DO_NEXT_TRACK,
    DO_PREV_TRACK,
    SEEK_TRACK,
    ADD_TRACK,
    DEL_TRACK,
    QUIT,
    // Custom events for asynchronous track loading
    TRACK_LOAD_REQUEST,  // Sent to loader thread (via queue)
    TRACK_LOAD_SUCCESS,  // Sent from loader thread to main thread
    TRACK_LOAD_FAILURE,  // Sent from loader thread to main thread
    START_FIRST_TRACK    // Sent from playlist populator to main thread
};

enum class PlayerState {
    Stopped,
    Playing,
    Paused
};

// system includes - runtime libraries
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>
#include <exception>
#include <typeinfo>
#include <thread>
#include <memory>
#include <unordered_set>
#include <iomanip>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <atomic>
#include <complex>
#include <fstream>

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

#ifdef __cplusplus
    #include <cstdlib>
    #include <cmath>
    #include <cstdio>
    #include <cstring>
#else
    #include <stdlib.h>
    #include <math.h>
    #include <stdio.h>
#endif

#include <sys/stat.h>

// system includes - third-party libraries
#if defined(_WIN32)
#define _UNICODE
#define UNICODE
#include <windows.h>
#include <initguid.h>
#include <tchar.h>
#include <lm.h>
#include <shlobj.h>
#include <SDL_syswm.h>
#endif
#if defined(__APPLE__) 
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_mutex.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#endif
#include <FLAC++/decoder.h>
#include <mpg123.h>
#include <vorbis/vorbisfile.h>
#include <opus/opusfile.h>
#include <taglib/tag.h>
#include <taglib/fileref.h>

// local includes
#include "exceptions.h"
#include "system.h"
#include "stream.h"
#include "libmpg123w.h"
#include "vorbisw.h"
#include "opusw.h"
#include "flac.h"
#include "nullstream.h"
#include "mediafile.h"
#include "fft.h"
#include "fft_draw.h"
#include "audio.h"
#include "about.h"
#include "persistentstorage.h"
#include "track.h"
#include "song.h"
#include "scrobble.h"
#include "playlist.h"
#include "rect.h"
#include "surface.h"
#include "display.h"
#include "truetype.h"
#include "font.h"
#include "Widget.h"
#include "player.h"

#ifdef DEBUG
#define PSYMP3_DATADIR "/usr/local/share/psymp3/data"
#endif /* DEBUG */

#endif // __PSYMP3_H__
