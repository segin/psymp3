#
# Makefile.am - automake input for the raw AC-3 / E-AC-3 demuxer
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-demuxer-ac3.a

libpsymp3_demuxer_ac3_a_SOURCES = \
	AC3NullDemuxer.cpp

AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
