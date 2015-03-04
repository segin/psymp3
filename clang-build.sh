#!/bin/sh
# A hackish way of building it all. Deprecated; DO NOT USE!
# Correction: Better than autotools, maybe it should be used...


# i needed the following packages for debian
#   libSDL-dev
#   libmpg123-dev
#   libSDL-gfx1.2-dev
#   libSDL-ttf2.0-dev
#   libvorbis-dev
#   libflac++-dev
#   libtag1-dev

clang -O0 -ggdb3 `pkg-config --cflags sdl libmpg123 taglib vorbisfile SDL_gfx` -o psymp3 -DPSYMP3_DATADIR="\"/usr/share/psymp3\"" -Iinclude src/psymp3.final.cpp -lSDL_ttf `pkg-config --libs sdl libmpg123 taglib vorbisfile SDL_gfx`

