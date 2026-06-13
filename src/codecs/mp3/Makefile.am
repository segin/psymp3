#
# Makefile.am - automake input for MP3 codec
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

# Use same flags as parent to ensure consistent compilation
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(SPEEX_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(AAC_CFLAGS) $(G722_CFLAGS) -Wall -Werror

noinst_LIBRARIES = libpsymp3-codec-mp3.a

libpsymp3_codec_mp3_a_SOURCES = \
	MiniMP3Codec.cpp
