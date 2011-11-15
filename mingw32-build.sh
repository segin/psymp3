#!/bin/sh
# mingw32 build method. Requires that you provide --prefix=$HOME/mingw32 to 
# dependency ./configure passes (that is, to SDL and friends) 

# Toolchain prefix.
TCPFX=mingw32-

${TCPFX}windres res/psymp3.rc -o res/psymp3-res.o -O coff
${TCPFX}gcc src/sqlite3.c -Iinclude -c -o sqlite3-pecoff.o
${TCPFX}g++ `PKG_CONFIG_PATH="${HOME}/mingw32/lib/pkgconfig" pkg-config --cflags --libs sdl libmpg123 taglib libvisual-0.4 vorbisfile SDL_gfx` -o psymp3.exe -Iinclude src/*.cpp sqlite3-pecoff.o res/psymp3-res.o -lSDL_ttf -DPSYMP3_DATADIR=\".\" ~/mingw32/lib/*.dll.a
