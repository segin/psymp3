#
# Makefile.am - automake input for core compression library
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

noinst_LIBRARIES = libpsymp3-core-compression.a

libpsymp3_core_compression_a_SOURCES = \
	LZ77.cpp

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) \
	$(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) \
	$(DBUS_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) \
	$(OGG_CFLAGS) $(LIBMPG123_CFLAGS)
