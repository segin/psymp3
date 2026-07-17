#
# Makefile.am - automake input for the Apple Lossless (ALAC) codec
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(SPEEX_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(AAC_CFLAGS) $(G722_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-codec-alac.a
libpsymp3_codec_alac_a_SOURCES = \
	ALACCodec.cpp

# Apple's ALAC sources use multi-character constants ('alac', etc.). The
# in-file `#pragma GCC diagnostic ignored "-Wmultichar"` suppresses this on
# newer GCC/clang, but GCC 10's lexer emits the warning before the pragma takes
# effect (e.g. on NetBSD i386), so disable it on the command line too.
AM_CXXFLAGS = $(PSYMP3_CXXFLAGS) -Wno-multichar
