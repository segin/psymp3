#
# Makefile.am - automake input for ISO demuxer submodule
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

noinst_LIBRARIES = libpsymp3-demuxer-iso.a

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(LIBMPG123_CFLAGS) -Wall -Werror

libpsymp3_demuxer_iso_a_SOURCES = \
	ISODemuxer.cpp \
	BoxParser.cpp \
	ComplianceValidator.cpp \
	ErrorRecovery.cpp \
	FragmentHandler.cpp \
	MetadataExtractor.cpp \
	SampleTableManager.cpp \
	SeekingEngine.cpp \
	StreamManager.cpp
