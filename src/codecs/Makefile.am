#
# Makefile.am - automake input for codecs subdirectory
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

SUBDIRS = pcm

noinst_LIBRARIES = libcodecs.a

AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) $(DBUS_CFLAGS) $(OPUS_CFLAGS) $(OGG_CFLAGS) $(VORBIS_CFLAGS) $(LIBMPG123_CFLAGS) -Wall -Werror

libcodecs_a_SOURCES = \
	AudioCodec.cpp \
	CodecRegistration.cpp \
	CodecRegistry.cpp \
	OggCodecs.cpp

if HAVE_FLAC
SUBDIRS += flac
endif

if HAVE_OPUS
SUBDIRS += opus
endif

if HAVE_VORBIS
SUBDIRS += vorbis
endif

if HAVE_MP3
SUBDIRS += mp3
endif
