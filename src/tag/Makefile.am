#
# Makefile.am - Tag subsystem build configuration
# This file is part of PsyMP3.
# Copyright © 2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#

noinst_LIBRARIES = libpsymp3-tag.a

# Use generic name to produce Tag.o instead of libpsymp3_tag_a-Tag.o
libpsymp3_tag_a_SOURCES = \
	Tag.cpp \
	NullTag.cpp \
	VorbisCommentTag.cpp \
	ID3v1Tag.cpp \
	ID3v2Tag.cpp \
	ID3v2Utils.cpp \
	MergedID3Tag.cpp \
	TagFactory.cpp

# Use AM_CPPFLAGS (not per-target) to avoid object file renaming
AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@SDL_CFLAGS@ \
	@TAGLIB_CFLAGS@ \
	@FREETYPE_CFLAGS@ \
	@OPENSSL_CFLAGS@ \
	@CURL_CFLAGS@ \
	@DBUS_CFLAGS@ \
	@OPUS_CFLAGS@

AM_CXXFLAGS = -Wall -Werror

