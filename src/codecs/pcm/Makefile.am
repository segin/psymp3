#
# Makefile.am - automake input for PCM codecs
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

# Use same flags as parent to ensure consistent compilation
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) -Wall -Werror

# Add optional codec flags if available
if HAVE_MP3
AM_CPPFLAGS += $(LIBMPG123_CFLAGS)
endif

if HAVE_VORBIS
AM_CPPFLAGS += $(VORBIS_CFLAGS)
endif

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS)
endif

if HAVE_OGG
AM_CPPFLAGS += $(OGG_CFLAGS)
endif

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
endif

noinst_LIBRARIES = libpcmcodecs.a

libpcmcodecs_a_SOURCES = \
	PCMCodecs.cpp \
	SimplePCMCodec.cpp \
	ALawCodec.cpp \
	MuLawCodec.cpp
