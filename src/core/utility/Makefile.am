#
# Makefile.am - automake input for core/utility subsystem
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

noinst_LIBRARIES = libpsymp3-core-utility.a

libpsymp3_core_utility_a_SOURCES = \
	UTF8Util.cpp \
	XMLUtil.cpp \
	utility.cpp

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(VORBIS_CFLAGS) $(OGG_CFLAGS) $(LIBMPG123_CFLAGS)
