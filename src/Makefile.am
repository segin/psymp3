#
# Makefile.am - automake input
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that
# the above copyright notice and this permission notice appear in all
# copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT of OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

datadir=$(prefix)/share/psymp3/data

# Subdirectories (conditionally built)
SUBDIRS = lastfm mpris io demuxer widget

AM_CPPFLAGS = $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) -DPSYMP3_DATADIR='"$(datadir)"' -Wall -Werror
psymp3_LDADD = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(WINSOCK_LIBS) codecs/libcodecs.a codecs/pcm/libpcmcodecs.a lastfm/liblastfm.a mpris/libmpris.a io/libio.a io/file/libfileio.a io/http/libhttpio.a demuxer/libdemuxer.a demuxer/iso/libisodemuxer.a demuxer/riff/libriffdemuxer.a demuxer/raw/librawdemuxer.a widget/foundation/libwidgetfoundation.a widget/windowing/libwidgetwindowing.a widget/ui/libwidgetui.a

# Optional codec flags and libraries
if HAVE_FLAC
if !HAVE_NATIVE_FLAC
# Only link libFLAC if not using native decoder
AM_CPPFLAGS += $(FLAC_CFLAGS)
psymp3_LDADD += $(FLAC_LIBS)
endif
endif

if HAVE_MP3
AM_CPPFLAGS += $(LIBMPG123_CFLAGS)
psymp3_LDADD += $(LIBMPG123_LIBS)
endif

if HAVE_VORBIS
AM_CPPFLAGS += $(VORBIS_CFLAGS)
psymp3_LDADD += $(VORBIS_LIBS)
endif

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS)
psymp3_LDADD += $(OPUS_LIBS)
endif

if HAVE_OGG
AM_CPPFLAGS += $(OGG_CFLAGS)
psymp3_LDADD += $(OGG_LIBS)
endif

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
psymp3_LDADD += $(DBUS_LIBS)
endif

bin_PROGRAMS = psymp3

if FINAL_BUILD
# Final build mode - single compilation unit
psymp3_SOURCES = psymp3.final.cpp
else
# Regular build mode - individual source files
psymp3_SOURCES = \
 about.cpp audio.cpp BufferPool.cpp \
 debug.cpp display.cpp \
 exceptions.cpp fft.cpp fft_draw.cpp \
 font.cpp \
 RAIIFileHandle.cpp EnhancedAudioBufferPool.cpp EnhancedBufferPool.cpp \
 Label.cpp lyrics.cpp \
 main.cpp mediafile.cpp MemoryOptimizer.cpp MemoryPoolManager.cpp MemoryTracker.cpp BoundedBuffer.cpp \
 persistentstorage.cpp player.cpp playlist.cpp \
 rect.cpp \
 song.cpp stream.cpp surface.cpp \
 system.cpp \
 track.cpp truetype.cpp utility.cpp \
 XMLUtil.cpp

# Optional codec sources
if HAVE_FLAC
psymp3_LDADD += demuxer/flac/libflacdemuxer.a
if HAVE_NATIVE_FLAC
# Native FLAC decoder (no libFLAC dependency)
SUBDIRS += codecs
psymp3_LDADD += codecs/flac/libnativeflac.a
else
# libFLAC wrapper
SUBDIRS += codecs
psymp3_LDADD += codecs/flac/libflaccodec.a
endif
endif

if HAVE_MP3
SUBDIRS += codecs
psymp3_LDADD += codecs/mp3/libmp3codec.a
endif

if HAVE_VORBIS
SUBDIRS += codecs
psymp3_LDADD += codecs/vorbis/libvorbiscodec.a
endif

if HAVE_OPUS
SUBDIRS += codecs
psymp3_LDADD += codecs/opus/libopuscodec.a
endif

if HAVE_OGGDEMUXER
psymp3_LDADD += demuxer/ogg/liboggdemuxer.a
endif

# MPRIS sources now in mpris/ subdirectory

endif

# Windows resource file support
if HAVE_WINDRES
psymp3_LDADD += ../res/psymp3.res
endif

