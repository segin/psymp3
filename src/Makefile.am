#
# Makefile.am - automake input
# This file is part of PsyMP3.
# Copyright © 2011-2025 Kirn Gill <segin2005@gmail.com>
#
# PsyMP3 is free software. You may redistribute and/or modify it under
# the terms of the ISC License <https://opensource.org/licenses/ISC>
#
# Permission to use, copy, modify, and/or distribute this software for
# any purpose with or without fee is hereby granted, provided that
# the above copyright notice and this permission notice appear in all
# copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT of OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

datadir=$(prefix)/share/psymp3/data

AM_CPPFLAGS = $(DEPENDS_CFLAGS) -DPSYMP3_DATADIR=\"$(datadir)\"
psymp3_LDADD = $(DEPENDS_LIBS)

bin_PROGRAMS = psymp3
psymp3_SOURCES = about.cpp audio.cpp display.cpp exceptions.cpp fft.cpp\
 fft_draw.cpp flac.cpp font.cpp Label.cpp libmpg123.cpp main.cpp mediafile.cpp \
 player.cpp playlist.cpp opus.cpp rect.cpp scrobble.cpp song.cpp stream.cpp \
 surface.cpp system.cpp track.cpp truetype.cpp utility.cpp vorbis.cpp Widget.cpp \
 ChainedStream.cpp ToastNotification.cpp FadingWidget.cpp

# psymp3_CXXFLAGS = $(AM_CXXFLAGS)

