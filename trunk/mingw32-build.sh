#!/bin/sh
# mingw32 build method. Requires that you provide --prefix=$HOME/mingw32 to 
# dependency ./configure passes (that is, to SDL and friends) 

mingw32-g++ `PKG_CONFIG_PATH="${HOME}/mingw32/lib/pkgconfig" pkg-config --cflags --libs sdl libmpg123 taglib libvisual-0.4 vorbisfile SDL_gfx` -o psymp3.exe -Iinclude src/*.cpp -lSDL_ttf -DPSYMP3_DATADIR=\".\" ~/mingw32/lib/*.dll.a
