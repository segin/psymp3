#!/bin/sh
mingw32-g++ `PKG_CONFIG_PATH="/usr/home/segin/mingw32/lib/pkgconfig" pkg-config --cflags --libs sdl libmpg123 taglib libvisual-0.4 vorbisfile SDL_gfx` -o psymp3.exe -Iinclude src/*.cpp -lSDL_ttf -DPSYMP3_DATADIR=\".\" ~/mingw32/lib/*.dll.a
