#!/bin/sh
# Historical reference: the original i686 mingw32 cross-build one-liner, from
# before the autotools build existed. Kept for reference only -- it names
# dependencies PsyMP3 no longer uses (SDL 1.2, libmpg123, libvisual, vorbisfile).
# Set PREFIX to wherever the cross-compiled dependencies live.
PREFIX="${PREFIX:-$HOME/build/target}"

PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig/" i686-w64-mingw32-g++ src/*.cpp -o psymp3.exe \
	`PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig/" pkg-config --cflags --libs sdl libmpg123 libvisual-0.4 vorbisfile` \
	`"$PREFIX/bin/taglib-config" --cflags --libs` \
	-Iinclude -lSDL_ttf -U_GNU_SOURCE -DPSYMP3_DATADIR=\".\" -DUNICODE -lole32 psymp3-res.o
