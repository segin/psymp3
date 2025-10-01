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

AM_CPPFLAGS = $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) -DPSYMP3_DATADIR='"$(datadir)"' -Wall -Werror
psymp3_LDADD = $(SDL_LIBS) $(TAGLIB_LIBS) $(FREETYPE_LIBS) $(OPENSSL_LIBS) $(CURL_LIBS) $(WINSOCK_LIBS)

# Optional codec flags and libraries
if HAVE_FLAC
AM_CPPFLAGS += $(FLAC_CFLAGS)
psymp3_LDADD += $(FLAC_LIBS)
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
 about.cpp ApplicationWidget.cpp audio.cpp AudioCodec.cpp BufferPool.cpp ButtonWidget.cpp \
 ChainedStream.cpp ChunkDemuxer.cpp CodecRegistration.cpp CodecRegistry.cpp debug.cpp \
 DemuxedStream.cpp Demuxer.cpp DemuxerExtensibility.cpp DemuxerFactory.cpp DemuxerPlugin.cpp DemuxerRegistry.cpp display.cpp \
 DrawableWidget.cpp exceptions.cpp FadingWidget.cpp fft.cpp fft_draw.cpp \
 FileIOHandler.cpp font.cpp HTTPClient.cpp HTTPIOHandler.cpp IOHandler.cpp \
 RAIIFileHandle.cpp EnhancedAudioBufferPool.cpp EnhancedBufferPool.cpp \
 ISODemuxer.cpp ISODemuxerBoxParser.cpp ISODemuxerComplianceValidator.cpp ISODemuxerErrorRecovery.cpp ISODemuxerFragmentHandler.cpp \
 ISODemuxerMetadataExtractor.cpp ISODemuxerSampleTableManager.cpp ISODemuxerSeekingEngine.cpp ISODemuxerStreamManager.cpp \
 Label.cpp LastFM.cpp LayoutWidget.cpp lyrics.cpp LyricsWidget.cpp \
 main.cpp MainUIWidget.cpp MediaFactory.cpp mediafile.cpp MemoryOptimizer.cpp MemoryPoolManager.cpp MemoryTracker.cpp BoundedBuffer.cpp ModernStream.cpp \
 PCMCodecs.cpp persistentstorage.cpp player.cpp PlayerProgressBarWidget.cpp playlist.cpp \
 ProgressBarBracketWidget.cpp ProgressBarFrameWidget.cpp RawAudioDemuxer.cpp rect.cpp scrobble.cpp \
 song.cpp SpectrumAnalyzerWidget.cpp stream.cpp StreamingManager.cpp surface.cpp \
 system.cpp TagLibIOHandlerAdapter.cpp TitlebarWidget.cpp ToastNotification.cpp ToastWidget.cpp \
 track.cpp TransparentWindowWidget.cpp truetype.cpp URI.cpp utility.cpp \
 wav.cpp Widget.cpp WindowFrameWidget.cpp WindowWidget.cpp XMLUtil.cpp

# Optional codec sources
if HAVE_FLAC
psymp3_SOURCES += flac.cpp FLACDemuxer.cpp FLACCodec.cpp FLACPerformanceBenchmark.cpp FLACRFCValidator.cpp
endif

if HAVE_MP3
psymp3_SOURCES += libmpg123.cpp
endif

if HAVE_VORBIS
psymp3_SOURCES += vorbis.cpp
endif

if HAVE_OPUS
psymp3_SOURCES += opus.cpp OpusCodec.cpp
endif

if HAVE_OGGDEMUXER
psymp3_SOURCES += OggDemuxer.cpp OggCodecs.cpp
endif

if HAVE_DBUS
psymp3_SOURCES += MPRISTypes.cpp DBusConnectionManager.cpp PropertyManager.cpp MethodHandler.cpp SignalEmitter.cpp MPRISManager.cpp
endif

# G.711 codec sources
if HAVE_ALAW
psymp3_SOURCES += ALawCodec.cpp
endif

if HAVE_MULAW
psymp3_SOURCES += MuLawCodec.cpp
endif
endif

# Windows resource file support
if HAVE_WINDRES
psymp3_LDADD += ../res/psymp3.res
endif

