#!/bin/sh

g++ `pkg-config --cflags --libs sdl libmpg123 taglib libvisual-0.4 vorbisfile SDL_gfx` -o psymp3 -Iinclude src/*.cpp -lSDL_ttf
