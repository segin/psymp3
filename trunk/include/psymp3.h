/*
 * psymp3.h - main include for all other source files.
 * This file is part of PsyMP3.
 * Copyright © 2011 Kirn Gill <segin2005@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __PSYMP3_H__
#define __PSYMP3_H__

// defines
#define PSYMP3_VERSION "2-CURRENT"
#define PSYMP3_MAINTAINER "Kirn Gill <segin2005@gmail.com>"

#define RUN_GUI_ITERATION 0xfe0f

// system includes
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#ifdef __cplusplus
    #include <cstdlib>
#else
    #include <stdlib.h>
#endif
#ifdef __APPLE__
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>
#else
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>
#endif
#include <mpg123.h>
#include <vorbis/vorbisfile.h>
#include <taglib/tag.h>
#include <taglib/fileref.h>

// local includes
#include "stream.h"
#include "libmpg123w.h"
#include "vorbisw.h"
#include "audio.h"
#include "about.h"
#include "track.h"
#include "song.h"
#include "scrobble.h"
#include "playlist.h"
#include "rect.h"
#include "surface.h"
#include "display.h"
#include "truetype.h"
#include "font.h"
#include "player.h"

// global singletons

#endif // __PSYMP3_H__
