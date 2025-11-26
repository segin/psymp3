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
#include <ostream>

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

// Stream operator for PlayerState (for testing)
inline std::ostream& operator<<(std::ostream& os, PlayerState state) {
    switch (state) {
        case PlayerState::Stopped: return os << "Stopped";
        case PlayerState::Playing: return os << "Playing";
        case PlayerState::Paused: return os << "Paused";
        default: return os << "Unknown";
    }
}

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
#include <random>
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
#include <psapi.h>
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
#include <opus/opus.h>
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

// Widget system - Foundation
#include "widget/foundation/Widget.h"
#include "widget/foundation/DrawableWidget.h"
#include "widget/foundation/LayoutWidget.h"
#include "widget/foundation/FadingWidget.h"

// Widget system - Windowing
#include "widget/windowing/TitlebarWidget.h"
#include "widget/windowing/WindowFrameWidget.h"
#include "widget/windowing/WindowWidget.h"
#include "widget/windowing/TransparentWindowWidget.h"

// Widget system - UI
#include "widget/ui/ButtonWidget.h"
#include "widget/ui/SpectrumAnalyzerWidget.h"
#include "widget/ui/PlayerProgressBarWidget.h"
#include "widget/ui/ProgressBarFrameWidget.h"
#include "widget/ui/ProgressBarBracketWidget.h"
#include "widget/ui/MainUIWidget.h"
#include "widget/ui/ApplicationWidget.h"
#include "widget/ui/ToastWidget.h"
#include "widget/ui/LyricsWidget.h"

// Bring widget types into global namespace for backward compatibility
using PsyMP3::Widget::Foundation::Widget;
using PsyMP3::Widget::Foundation::DrawableWidget;
using PsyMP3::Widget::Foundation::LayoutWidget;
using PsyMP3::Widget::Foundation::FadingWidget;
using PsyMP3::Widget::Windowing::TitlebarWidget;
using PsyMP3::Widget::Windowing::WindowFrameWidget;
using PsyMP3::Widget::Windowing::WindowWidget;
using PsyMP3::Widget::Windowing::WindowEvent;
using PsyMP3::Widget::Windowing::WindowEventData;
using PsyMP3::Widget::Windowing::TransparentWindowWidget;
using PsyMP3::Widget::UI::ButtonWidget;
using PsyMP3::Widget::UI::ButtonSymbol;
using PsyMP3::Widget::UI::SpectrumAnalyzerWidget;
using PsyMP3::Widget::UI::PlayerProgressBarWidget;
using PsyMP3::Widget::UI::ProgressBarFrameWidget;
using PsyMP3::Widget::UI::ProgressBarLeftBracketWidget;
using PsyMP3::Widget::UI::ProgressBarRightBracketWidget;
using PsyMP3::Widget::UI::MainUIWidget;
using PsyMP3::Widget::UI::ApplicationWidget;
using PsyMP3::Widget::UI::ToastWidget;
using PsyMP3::Widget::UI::LyricsWidget;

#include "Label.h"
#include "ZOrder.h"
#include "widget/ui/ToastNotification.h"

// Bring ToastNotification into global namespace after its header is included
using PsyMP3::Widget::UI::ToastNotification;

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

// I/O Handler subsystem - Base
#include "io/http/HTTPClient.h"
#include "io/IOHandler.h"
#include "io/file/FileIOHandler.h"
#include "io/http/HTTPIOHandler.h"
#include "io/TagLibIOHandlerAdapter.h"
#include "io/URI.h"

// Using declarations for I/O classes for backward compatibility
using PsyMP3::IO::IOHandler;
using PsyMP3::IO::File::FileIOHandler;
using PsyMP3::IO::HTTP::HTTPIOHandler;
using PsyMP3::IO::HTTP::HTTPClient;
using PsyMP3::IO::TagLibIOHandlerAdapter;
using PsyMP3::IO::URI;

#include "stream.h"
#include "BoundedQueue.h"

// Demuxer subsystem - Core (defines StreamInfo, MediaChunk, etc.)
#include "demuxer/Demuxer.h"
#include "demuxer/DemuxerFactory.h"
#include "demuxer/MediaFactory.h"
#include "demuxer/DemuxerRegistry.h"
#include "demuxer/DemuxerPlugin.h"
#include "demuxer/DemuxerExtensibility.h"
#include "demuxer/ChunkDemuxer.h"

// Using declarations for core demuxer types (needed by other headers)
using PsyMP3::Demuxer::Demuxer;
using PsyMP3::Demuxer::StreamInfo;
using PsyMP3::Demuxer::MediaChunk;
using PsyMP3::Demuxer::DemuxerError;
using PsyMP3::Demuxer::DemuxerErrorRecovery;
using PsyMP3::Demuxer::BufferPool;
using PsyMP3::Demuxer::ChunkDemuxer;
using PsyMP3::Demuxer::Chunk;
using PsyMP3::Demuxer::FormatSignature;
using PsyMP3::Demuxer::DemuxerFactory;
using PsyMP3::Demuxer::DemuxerRegistry;

// I/O Handler subsystem - Advanced (depends on Demuxer and BoundedQueue)
#include "io/StreamingManager.h"

// Using declaration for StreamingManager (after include)
using PsyMP3::IO::StreamingManager;

// Codec architecture (depends on Demuxer types)
#include "codecs/AudioCodec.h"
#include "codecs/CodecRegistry.h"
#include "codecs/CodecRegistration.h"
#include "codecs/pcm/PCMCodecs.h"
// Bring PCM codec types into global namespace for compatibility
using PsyMP3::Codec::PCM::PCMCodec;
using PsyMP3::Codec::PCM::MP3PassthroughCodec;
#ifdef ENABLE_MULAW_CODEC
#include "codecs/pcm/MuLawCodec.h"
using PsyMP3::Codec::PCM::MuLawCodec;
#endif
#ifdef ENABLE_ALAW_CODEC
#include "codecs/pcm/ALawCodec.h"
using PsyMP3::Codec::PCM::ALawCodec;
#endif
#include "demuxer/DemuxedStream.h"

// Demuxer subsystem - Raw Audio
#include "demuxer/raw/RawAudioDemuxer.h"

// Codec includes needed by OggCodecs.h (must come before OggCodecs.h)
#ifdef HAVE_VORBIS
#include "codecs/vorbis/VorbisCodec.h"
// Bring Vorbis codec types into global namespace for compatibility
using PsyMP3::Codec::Vorbis::Vorbis;
using PsyMP3::Codec::Vorbis::VorbisCodec;
#endif
#ifdef HAVE_OPUS
#include "codecs/opus/opusw.h"
#include "codecs/opus/OpusCodec.h"
// Bring Opus codec types into global namespace for compatibility
using PsyMP3::Codec::Opus::OpusCodec;
using PsyMP3::Codec::Opus::OpusHeader;
#endif

// Demuxer subsystem - Ogg
#ifdef HAVE_OGGDEMUXER
#include "demuxer/ogg/OggDemuxer.h"
// Bring Ogg demuxer types into global namespace for compatibility
using PsyMP3::Demuxer::Ogg::OggDemuxer;
using PsyMP3::Demuxer::Ogg::OggStream;
using PsyMP3::Demuxer::Ogg::OggPacket;
// OggCodecs.h needs full definitions of codec types - include them here
// These must be included regardless of individual codec flags since OggCodecs references them
#include "codecs/vorbis/VorbisCodec.h"
#include "codecs/opus/opusw.h"
#include "codecs/opus/OpusCodec.h"
#include "codecs/OggCodecs.h"
#endif

// Demuxer subsystem - ISO/MP4
#include "demuxer/iso/ISODemuxerErrorRecovery.h"
#include "demuxer/iso/ISODemuxerBoxParser.h"
#include "demuxer/iso/ISODemuxerSampleTableManager.h"
#include "demuxer/iso/ISODemuxerFragmentHandler.h"
#include "demuxer/iso/ISODemuxerMetadataExtractor.h"
#include "demuxer/iso/ISODemuxerStreamManager.h"
#include "demuxer/iso/ISODemuxerSeekingEngine.h"
#include "demuxer/iso/ISODemuxerComplianceValidator.h"
#include "demuxer/iso/ISODemuxer.h"
#include "demuxer/ModernStream.h"
#ifdef HAVE_MP3
#include "codecs/mp3/MP3Codec.h"
#endif
#ifdef HAVE_OPUS
using PsyMP3::Codec::Opus::OpusComments;
#endif
#include "demuxer/riff/wav.h"
#ifdef HAVE_FLAC
#include "codecs/flac/FLACRFC9639.h"
#include "codecs/flac/FLACRFCValidator.h"
#include "demuxer/flac/FLACDemuxer.h"
// Bring FLAC demuxer types into global namespace for compatibility
using PsyMP3::Demuxer::FLAC::FLACDemuxer;
using PsyMP3::Demuxer::FLAC::FLACStreamInfo;
#ifdef HAVE_NATIVE_FLAC
// Native FLAC codec
#include "codecs/flac/FLACError.h"
#include "codecs/flac/BitstreamReader.h"
#include "codecs/flac/CRCValidator.h"
#include "codecs/flac/FrameParser.h"
#include "codecs/flac/ResidualDecoder.h"
#include "codecs/flac/SubframeDecoder.h"
#include "codecs/flac/ChannelDecorrelator.h"
#include "codecs/flac/SampleReconstructor.h"
#include "codecs/flac/MetadataParser.h"
#include "codecs/flac/MD5Validator.h"
#include "codecs/flac/NativeFLACCodec.h"
// Bring Native FLAC codec types into global namespace for compatibility
using PsyMP3::Codec::FLAC::FLACCodec;
using PsyMP3::Codec::FLAC::FLACError;
using PsyMP3::Codec::FLAC::FLACException;
#include "codecs/flac/FLACPerformanceBenchmark.h"
#else
// libFLAC wrapper codec
#include "codecs/flac.h"
#include "codecs/FLACCodec.h"
#include "codecs/flac/FLACPerformanceBenchmark.h"
#endif
#endif
#include "demuxer/ChainedStream.h"

// Using declarations for stream types (after their headers are included)
using PsyMP3::Demuxer::DemuxedStream;
using PsyMP3::Demuxer::ModernStream;
using PsyMP3::Demuxer::ChainedStream;

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

// Using declarations for backward compatibility - XML subsystem
using PsyMP3::XML::XMLUtil;

// Last.fm scrobbling
#include "lastfm/scrobble.h"
#include "lastfm/LastFM.h"

// Using declarations for backward compatibility
using PsyMP3::LastFM::LastFM;
using PsyMP3::LastFM::Scrobble;

#include "playlist.h"
#include "player.h"
#include "mpris/MPRISTypes.h"
#include "mpris/MPRISDebugMacros.h"
#include "mpris/DBusConnectionManager.h"
#include "mpris/PropertyManager.h"
#include "mpris/MethodHandler.h"
#include "mpris/SignalEmitter.h"
#include "mpris/MPRISManager.h"

// Portable branch prediction macros

#ifdef DEBUG
#define PSYMP3_DATADIR "/usr/local/share/psymp3/data"
#endif /* DEBUG */

#endif // __PSYMP3_H__
