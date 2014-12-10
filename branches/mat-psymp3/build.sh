#!/bin/sh
# A hackish way of building it all. Deprecated; DO NOT USE!

g++ `pkg-config --cflags --libs sdl libmpg123 taglib libvisual-0.4 vorbisfile SDL_gfx` -o psymp3 -Iinclude src/*.cpp -lSDL_ttf
