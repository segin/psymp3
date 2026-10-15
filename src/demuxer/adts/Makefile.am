#
# Makefile.am - automake input for the ADTS (raw AAC) demuxer
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-demuxer-adts.a

libpsymp3_demuxer_adts_a_SOURCES = \
	ADTSDemuxer.cpp

AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
