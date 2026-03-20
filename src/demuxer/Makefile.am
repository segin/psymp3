#
# Makefile.am - automake input for demuxer submodule
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

SUBDIRS = iso riff raw flac ogg

noinst_LIBRARIES = libpsymp3-demuxer.a

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(LIBMPG123_CFLAGS) -Wall -Werror

libpsymp3_demuxer_a_SOURCES = \
	ChainedStream.cpp \
	ChunkDemuxer.cpp \
	Demuxer.cpp \
	DemuxedStream.cpp \
	DemuxerExtensibility.cpp \
	DemuxerFactory.cpp \
	DemuxerPlugin.cpp \
	DemuxerRegistry.cpp \
	MediaFactory.cpp \
	ModernStream.cpp
