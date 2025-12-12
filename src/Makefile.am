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
# NOTE: When adding or removing source files, update psymp3.final.cpp
# to keep the final/unity build in sync. See AGENTS.md for details.
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

datadir=$(prefix)/share/psymp3/data

# Subdirectories - only built in regular mode (final build includes all sources directly)
if !FINAL_BUILD
SUBDIRS = codecs lastfm mpris util io demuxer widget core
endif

# Include path for psymp3.h and other headers
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) -DPSYMP3_DATADIR='"$(datadir)"' -Wall -Werror

# Optional codec flags
if HAVE_FLAC
if !HAVE_NATIVE_FLAC
AM_CPPFLAGS += $(FLAC_CFLAGS)
endif
endif

if HAVE_MP3
AM_CPPFLAGS += $(LIBMPG123_CFLAGS)
endif

if HAVE_VORBIS
AM_CPPFLAGS += $(VORBIS_CFLAGS)
endif

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS)
endif

if HAVE_OGG
AM_CPPFLAGS += $(OGG_CFLAGS)
endif

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
endif

# Define optional codec/demuxer libraries
CODEC_LIBS =
DEMUXER_LIBS =

if HAVE_FLAC
DEMUXER_LIBS += demuxer/flac/libflacdemuxer.a
if HAVE_NATIVE_FLAC
CODEC_LIBS += codecs/flac/libnativeflac.a
else
CODEC_LIBS += codecs/flac/libflaccodec.a
endif
endif

if HAVE_MP3
CODEC_LIBS += codecs/mp3/libmp3codec.a
endif

if HAVE_VORBIS
CODEC_LIBS += codecs/vorbis/libvorbiscodec.a
endif

if HAVE_OPUS
CODEC_LIBS += codecs/opus/libopuscodec.a
endif

if HAVE_OGGDEMUXER
DEMUXER_LIBS += demuxer/ogg/liboggdemuxer.a
endif

# System libraries (always needed)
psymp3_LDADD = \
	$(SDL_LIBS) \
	$(TAGLIB_LIBS) \
	$(FREETYPE_LIBS) \
	$(OPENSSL_LIBS) \
	$(CURL_LIBS) \
	$(WINSOCK_LIBS)

# Optional codec system libraries (must come after our libraries)
if HAVE_FLAC
if !HAVE_NATIVE_FLAC
psymp3_LDADD += $(FLAC_LIBS)
endif
endif

if HAVE_MP3
psymp3_LDADD += $(LIBMPG123_LIBS)
endif

if HAVE_VORBIS
psymp3_LDADD += $(VORBIS_LIBS)
endif

if HAVE_OPUS
psymp3_LDADD += $(OPUS_LIBS)
endif

if HAVE_OGG
psymp3_LDADD += $(OGG_LIBS)
endif

if HAVE_DBUS
psymp3_LDADD += $(DBUS_LIBS)
endif

bin_PROGRAMS = psymp3

if FINAL_BUILD
# Final build mode - single compilation unit
# All sources are #included in psymp3.final.cpp, no static libraries needed
psymp3_SOURCES = psymp3.final.cpp
else
# Regular build mode - individual source files
psymp3_SOURCES = \
 about.cpp audio.cpp \
 debug.cpp display.cpp \
 exceptions.cpp fft.cpp fft_draw.cpp \
 font.cpp \
 lyrics.cpp \
 main.cpp mediafile.cpp \
 persistentstorage.cpp player.cpp playlist.cpp \
 stream.cpp surface.cpp \
 system.cpp \
 track.cpp truetype.cpp

# Link static libraries from subdirectories (regular build only)
psymp3_LDADD += \
	widget/ui/libwidgetui.a \
	widget/windowing/libwidgetwindowing.a \
	widget/foundation/libwidgetfoundation.a \
	demuxer/libdemuxer.a \
	io/http/libhttpio.a \
	io/file/libfileio.a \
	io/libio.a \
	lastfm/liblastfm.a \
	mpris/libmpris.a \
	util/libutil.a \
	core/libpsymp3-core.a \
	core/utility/libpsymp3-core-utility.a \
	codecs/libcodecs.a \
	$(CODEC_LIBS) \
	$(DEMUXER_LIBS) \
	demuxer/iso/libisodemuxer.a \
	demuxer/riff/libriffdemuxer.a \
	demuxer/raw/librawdemuxer.a \
	demuxer/libdemuxer.a \
	codecs/pcm/libpcmcodecs.a \
	codecs/libcodecs.a
endif

# Windows resource file support
if HAVE_WINDRES
psymp3_LDADD += ../res/psymp3.res
endif

