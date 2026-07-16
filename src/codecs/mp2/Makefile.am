#
# Makefile.am - automake input for the MPEG Layer II codec (kjmp2)
# This file is part of PsyMP3.
# Copyright © 2026 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(SPEEX_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(AAC_CFLAGS) $(G722_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-codec-mp2.a
libpsymp3_codec_mp2_a_SOURCES = \
	MP2Codec.cpp

AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
