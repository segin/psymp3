#ifndef __PSYMP3_H__
#define __PSYMP3_H__


// system includes

#include <iostream>
#include <string>
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
#else
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_gfxPrimitives.h>
#endif

// local includes

#include "track.h"
#include "playlist.h"

#endif // __PSYMP3_H__
