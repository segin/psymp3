#
# Makefile.am - automake input for Vorbis codec
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

# Use same flags as parent to ensure consistent compilation
# Include all optional codec flags to match parent build environment
AM_CPPFLAGS = -I$(top_srcdir)/include $(SDL_CFLAGS) $(TAGLIB_CFLAGS) $(FREETYPE_CFLAGS) $(OPENSSL_CFLAGS) $(CURL_CFLAGS) -Wall -Werror

# Add optional codec flags if available

if HAVE_OPUS
AM_CPPFLAGS += $(OPUS_CFLAGS)
endif

if HAVE_OGG
AM_CPPFLAGS += $(OGG_CFLAGS)
endif

if HAVE_DBUS
AM_CPPFLAGS += $(DBUS_CFLAGS)
endif

noinst_LIBRARIES = libpsymp3-codec-vorbis.a

# stb_vorbis_impl.c compiles the vendored third_party/stb/stb_vorbis.c as a
# C object (its global typedefs/macros are hostile to C++ inclusion).
libpsymp3_codec_vorbis_a_SOURCES = \
	VorbisCodec.cpp \
	stb_vorbis_impl.c

# Project-owned hardening/ABI flags (see configure.ac PSYMP3_CXXFLAGS). The
# flag set is C-compatible, and stb_vorbis_impl.c must not skip the hardening
# (-fstack-protector-strong, -D_FORTIFY_SOURCE, -fPIE) the C++ objects get.
AM_CXXFLAGS = $(PSYMP3_CXXFLAGS)
AM_CFLAGS = $(PSYMP3_CXXFLAGS)
