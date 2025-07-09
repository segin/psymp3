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

#include <cstdint>

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
    START_FIRST_TRACK,   // Sent from playlist populator to main thread
    TRACK_PRELOAD_SUCCESS, // Sent from loader thread for a preloaded track
    TRACK_PRELOAD_FAILURE, // Sent from loader thread for a failed preload
    TRACK_SEAMLESS_SWAP, // Event to perform seamless track transition
    DO_SAVE_PLAYLIST,
    QUIT_APPLICATION,
    AUTOMATED_SKIP_TRACK
};

enum class PlayerState {
    Stopped,
    Playing,
    Paused
};

enum class LoopMode {
    None,
    One,
    All
};

//
// C++ Standard Library
#include <algorithm>
#include <atomic>
#include <complex>
#include <condition_variable>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <regex>
#include <string>
#include <sstream>
#include <thread>
#include <typeinfo>
#include <unordered_set>
#include <vector>
#include <optional>
#include <chrono>
#include <mutex>

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

// C Standard Library (wrapped)
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// System-specific headers
#include <sys/stat.h>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
// Windows socket compatibility macros
// Note: We can't use a simple macro for close() because it conflicts with file streams
// Instead, we'll handle socket closing in the socket-specific code
#define poll WSAPoll
#ifndef POLLIN
#define POLLIN POLLRDNORM
#endif
#ifndef POLLOUT  
#define POLLOUT POLLWRNORM
#endif
#define socklen_t int
#define ssize_t int
#ifndef EINPROGRESS
#define EINPROGRESS WSAEWOULDBLOCK
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
// Define fcntl constants before the function
#ifndef F_GETFL
#define F_GETFL 1
#endif
#ifndef F_SETFL
#define F_SETFL 2
#endif
// Windows doesn't have fcntl, we'll handle non-blocking sockets differently
inline int fcntl(int fd, int cmd, int flags = 0) {
    if (cmd == F_GETFL) return 0;
    if (cmd == F_SETFL && (flags & O_NONBLOCK)) {
        u_long mode = 1;
        return ioctlsocket(fd, FIONBIO, &mode);
    }
    return 0;
}

// Helper function to get socket error on Windows
inline int getSocketError() {
    return WSAGetLastError();
}

// Helper function to check if error is EINPROGRESS equivalent
inline bool isSocketInProgress(int error) {
    return error == WSAEWOULDBLOCK;
}

// Helper function to close sockets on Windows
inline int closeSocket(int sock) {
    return closesocket(sock);
}
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

// Unix socket error helpers for consistency
inline int getSocketError() {
    return errno;
}

inline bool isSocketInProgress(int error) {
    return error == EINPROGRESS;
}

// Helper function to close sockets on Unix (same as regular close)
inline int closeSocket(int sock) {
    return close(sock);
}
#endif
#if defined(_WIN32)
#define _UNICODE
#define UNICODE
#include <windows.h>
#include <initguid.h>
#include <tchar.h>
#include <lm.h>
#include <shlobj.h>
#include <SDL_syswm.h>
#elif defined(__linux__)
#include <sys/prctl.h>
#elif defined(__FreeBSD__)
#include <pthread.h>
#include <pthread_np.h>
#endif

// OpenSSL headers
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/md5.h>
#include <openssl/evp.h>

// OpenSSL forward declarations and typedefs
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct bio_st BIO;

// Third-party library headers
#if defined(__APPLE__)
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_mutex.h>
#else
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#endif
#include <FLAC++/decoder.h>
#include <mpg123.h>
#include <vorbis/vorbisfile.h>
#include <opus/opusfile.h>
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>

// Local project headers (in dependency order where possible)
#include "exceptions.h"
#include "rect.h"
#include "surface.h"
#include "display.h"
#include "truetype.h"
#include "Widget.h"
#include "Label.h"
#include "ButtonWidget.h"
#include "DrawableWidget.h"
#include "SpectrumAnalyzerWidget.h"
#include "MainUIWidget.h"
#include "ApplicationWidget.h"
#include "PlayerProgressBarWidget.h"
#include "ProgressBarFrameWidget.h"
#include "ProgressBarBracketWidget.h"
#include "LayoutWidget.h"
#include "TitlebarWidget.h"
#include "WindowFrameWidget.h"
#include "WindowWidget.h"
#include "ZOrder.h"
#include "TransparentWindowWidget.h"
#include "ToastWidget.h"
#include "ToastNotification.h"

// New demuxer/codec architecture
#include "Demuxer.h"
#include "AudioCodec.h"
#include "ChunkDemuxer.h"
#include "PCMCodecs.h"
#include "DemuxedStream.h"
#include "RawAudioDemuxer.h"
#include "OggDemuxer.h"
#include "OggCodecs.h"
#include "ISODemuxer.h"
#include "ModernStream.h"
#include "MediaFactory.h"
#include "FadingWidget.h"
#include "utility.h"
#include "system.h"
#include "IOHandler.h"
#include "FileIOHandler.h"
#include "HTTPIOHandler.h"
#include "TagLibIOStreamAdapter.h"
#include "URI.h"
#include "stream.h"
#include "libmpg123w.h"
#include "vorbisw.h"
#include "opusw.h"
#include "wav.h"
#include "flac.h"
#include "ChainedStream.h"
#include "nullstream.h"
#include "mediafile.h"
#include "fft.h"
#include "fft_draw.h"
#include "audio.h"
#include "about.h"
#include "persistentstorage.h"
#include "track.h"
#include "song.h"
#include "XMLUtil.h"
#include "HTTPClient.h"
#include "scrobble.h"
#include "LastFM.h"
#include "playlist.h"
#include "player.h"

// Portable branch prediction macros
#ifdef __GNUC__
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Zero-overhead debug output system for old hardware
class Debug {
public:
    static bool widget_blitting_enabled;
    static bool runtime_debug_enabled;
    
    static void setWidgetBlittingDebug(bool enabled) { widget_blitting_enabled = enabled; }
    static void setRuntimeDebug(bool enabled) { runtime_debug_enabled = enabled; }
    
    // Optimized debug functions that avoid parameter evaluation when disabled
    template<typename... Args>
    static inline void widgetBlit(Args&&... args) {
        // Branch prediction hint: debugging is usually disabled
        if (UNLIKELY(widget_blitting_enabled)) {
            ((std::cout << args), ...);
            std::cout << std::endl;
        }
    }
    
    template<typename... Args>
    static inline void runtime(Args&&... args) {
        // Branch prediction hint: debugging is usually disabled
        if (UNLIKELY(runtime_debug_enabled)) {
            ((std::cout << args), ...);
            std::cout << std::endl;
        }
    }
};

#ifdef DEBUG
#define PSYMP3_DATADIR "/usr/local/share/psymp3/data"
#endif /* DEBUG */

#endif // __PSYMP3_H__
