#
# Makefile.am - automake input for foundation widgets
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

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

noinst_LIBRARIES = libpsymp3-widget-foundation.a

libpsymp3_widget_foundation_a_SOURCES = \
	Widget.cpp \
	DrawableWidget.cpp \
	FadingWidget.cpp \
	LayoutWidget.cpp
