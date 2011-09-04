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
