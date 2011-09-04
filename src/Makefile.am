#
# Makefile.am - automake input
# This file is part of PsyMP3.
# Copyright © 2011 Kirn Gill <segin2005@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

datadir=$(prefix)/share/psymp3/data

AM_CPPFLAGS = $(DEPENDS_CFLAGS) -DPSYMP3_DATADIR=\"$(datadir)\"
AM_LDFLAGS = $(DEPENDS_LIBS) -lSDL_ttf

bin_PROGRAMS = psymp3
psymp3_SOURCES = about.cpp audio.cpp display.cpp exceptions.cpp \
 fft_draw.cpp font.cpp libmpg123.cpp main.cpp mediafile.cpp\
 mutex.cpp player.cpp \
 playlist.cpp rect.cpp scrobble.cpp song.cpp stream.cpp surface.cpp \
 system.cpp track.cpp truetype.cpp vorbis.cpp
