#!/bin/sh
# A hackish way of building it all. Deprecated; DO NOT USE!

# i needed the following packages for debian
#   libSDL-dev
#   libmpg123-dev
#   libSDL-gfx1.2-dev
#   libSDL-ttf2.0-dev
#   libvorbis-dev
#   libflac++-dev
#   libtag1-dev

g++ `pkg-config --cflags --libs sdl libmpg123 taglib vorbisfile SDL_gfx` -o psymp3 -DPSYMP3_DATADIR="\"/usr\"" -Iinclude src/psymp3.final.cpp -lSDL_ttf

