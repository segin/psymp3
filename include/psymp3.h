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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <stack>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include <optional>
#include <chrono>
#include <mutex>
#include <limits>

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
#include <sys/types.h>
#ifndef _WIN32
#include <sys/statfs.h>
#endif
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
#include <shobjidl.h>
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

// DBus headers (for MPRIS)
#ifdef HAVE_DBUS
#include <dbus/dbus.h>
#endif

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
#ifdef HAVE_FLAC
#include <FLAC++/decoder.h>
#endif
#ifdef HAVE_MP3
#include <mpg123.h>
#endif
#ifdef HAVE_OGGDEMUXER
#include <vorbis/vorbisfile.h>
#endif
#ifdef HAVE_OGGDEMUXER
#include <opus/opusfile.h>
#endif
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/tstring.h>
#include <taglib/tiostream.h>

// FreeType headers
#include <ft2build.h>
#include FT_FREETYPE_H

// Additional system headers needed by source files
#include <getopt.h>
#ifdef __SSE2__
#include <emmintrin.h>
#define HAVE_SSE2
#endif

#ifdef __ARM_NEON
#include <arm_neon.h>
#define HAVE_NEON
#endif

// cURL library
#include <curl/curl.h>

// Local project headers (in dependency order where possible)
#include "debug.h"
#include "exceptions.h"
#include "rect.h"
#include "surface.h"
#include "display.h"
#include "font.h"
#include "truetype.h"
#include "lyrics.h"
#include "Widget.h"
#include "Label.h"
#include "ButtonWidget.h"
#include "DrawableWidget.h"
#include "SpectrumAnalyzerWidget.h"
#include "LayoutWidget.h"
#include "PlayerProgressBarWidget.h"
#include "ProgressBarFrameWidget.h"
#include "ProgressBarBracketWidget.h"
#include "MainUIWidget.h"
#include "ApplicationWidget.h"
#include "TitlebarWidget.h"
#include "WindowFrameWidget.h"
#include "WindowWidget.h"
#include "ZOrder.h"
#include "TransparentWindowWidget.h"
#include "ToastWidget.h"
#include "ToastNotification.h"
#include "LyricsWidget.h"

// I/O and utility components (needed by other components)
#include "utility.h"
#include "system.h"
#include "BufferPool.h"
#include "BoundedBuffer.h"
#include "EnhancedBufferPool.h"
#include "EnhancedAudioBufferPool.h"
#include "MemoryTracker.h"
#include "MemoryOptimizer.h"
#include "MemoryPoolManager.h"
#include "RAIIFileHandle.h"
#include "HTTPClient.h"
#include "IOHandler.h"
#include "FileIOHandler.h"
#include "HTTPIOHandler.h"
#include "TagLibIOHandlerAdapter.h"
#include "URI.h"
#include "stream.h"

// New demuxer/codec architecture
#include "Demuxer.h"
#include "DemuxerFactory.h"
#include "AudioCodec.h"
#include "CodecRegistry.h"
#include "DemuxerRegistry.h"
#include "CodecRegistration.h"
#include "ChunkDemuxer.h"
#include "PCMCodecs.h"
#ifdef ENABLE_MULAW_CODEC
#include "MuLawCodec.h"
#endif
#ifdef ENABLE_ALAW_CODEC
#include "ALawCodec.h"
#endif
#include "DemuxedStream.h"
#include "RawAudioDemuxer.h"
#ifdef HAVE_OGGDEMUXER
#include "OggDemuxer.h"
#include "OggCodecs.h"
#endif
#include "ISODemuxerErrorRecovery.h"
#include "MemoryOptimizer.h"
#include "BoundedQueue.h"
#include "StreamingManager.h"
#include "ISODemuxerBoxParser.h"
#include "ISODemuxerSampleTableManager.h"
#include "ISODemuxerFragmentHandler.h"
#include "ISODemuxerMetadataExtractor.h"
#include "ISODemuxerStreamManager.h"
#include "ISODemuxerSeekingEngine.h"
#include "ISODemuxerComplianceValidator.h"
#include "ISODemuxer.h"
#include "ModernStream.h"
#include "MediaFactory.h"
#include "DemuxerPlugin.h"
#include "DemuxerExtensibility.h"
#include "FadingWidget.h"
#ifdef HAVE_MP3
#include "libmpg123w.h"
#endif
#ifdef HAVE_OGGDEMUXER
#include "vorbisw.h"
#endif
#ifdef HAVE_OGGDEMUXER
#include "opusw.h"
#endif
#include "wav.h"
#ifdef HAVE_FLAC
#include "flac.h"
#include "FLACDemuxer.h"
#include "FLACCodec.h"
#include "FLACPerformanceBenchmark.h"
#endif
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
#include "scrobble.h"
#include "LastFM.h"
#include "playlist.h"
#include "player.h"
#include "MPRISTypes.h"
#include "DBusConnectionManager.h"
#include "PropertyManager.h"
#include "MethodHandler.h"
#include "SignalEmitter.h"
#include "mpris.h"

// Portable branch prediction macros

#ifdef DEBUG
#define PSYMP3_DATADIR "/usr/local/share/psymp3/data"
#endif /* DEBUG */

#endif // __PSYMP3_H__
