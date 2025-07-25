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

AM_CPPFLAGS = $(DEPENDS_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) -DPSYMP3_DATADIR='"$(datadir)"' -Wall -Werror
psymp3_LDADD = $(DEPENDS_LIBS) $(FREETYPE_LIBS) $(OPENSSL_LIBS) $(WINSOCK_LIBS)

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
 about.cpp audio.cpp ChainedStream.cpp display.cpp exceptions.cpp fft.cpp \
 fft_draw.cpp flac.cpp font.cpp FadingWidget.cpp IOHandler.cpp Label.cpp \
 LastFM.cpp libmpg123.cpp lyrics.cpp LyricsWidget.cpp main.cpp mediafile.cpp opus.cpp player.cpp playlist.cpp \
 rect.cpp scrobble.cpp song.cpp stream.cpp surface.cpp system.cpp \
 TagLibIOHandlerAdapter.cpp TitlebarWidget.cpp ToastNotification.cpp track.cpp truetype.cpp URI.cpp utility.cpp \
 vorbis.cpp wav.cpp Widget.cpp DrawableWidget.cpp SpectrumAnalyzerWidget.cpp PlayerProgressBarWidget.cpp ProgressBarFrameWidget.cpp ProgressBarBracketWidget.cpp LayoutWidget.cpp WindowFrameWidget.cpp WindowWidget.cpp ApplicationWidget.cpp TransparentWindowWidget.cpp ToastWidget.cpp persistentstorage.cpp FileIOHandler.cpp HTTPIOHandler.cpp MainUIWidget.cpp ButtonWidget.cpp XMLUtil.cpp HTTPClient.cpp \
 Demuxer.cpp AudioCodec.cpp ChunkDemuxer.cpp PCMCodecs.cpp DemuxedStream.cpp RawAudioDemuxer.cpp OggDemuxer.cpp OggCodecs.cpp ISODemuxer.cpp ModernStream.cpp MediaFactory.cpp debug.cpp

if HAVE_DBUS
psymp3_SOURCES += mpris.cpp
endif
endif

# Windows resource file support
if HAVE_WINDRES
psymp3_LDADD += ../res/psymp3.res
endif

