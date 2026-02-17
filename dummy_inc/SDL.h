#pragma once
#include <stdint.h>
typedef uint32_t Uint32;
typedef uint16_t Uint16;
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int SDL_TimerID;
typedef int SDLKey;
struct SDL_keysym {};
struct SDL_MouseButtonEvent {};
struct SDL_MouseMotionEvent {};
struct SDL_UserEvent {};
struct SDL_Event {};
struct SDL_PixelFormat {};
struct SDL_Rect { int x, y, w, h; };
struct SDL_Surface {
    int w, h;
    int pitch;
    void* pixels;
    SDL_PixelFormat* format;
};
struct SDL_Color { Uint8 r,g,b,a; };
struct SDL_mutex {};
struct SDL_cond {};
struct SDL_Thread {};
struct SDL_Cursor {};

#define SDL_MUSTLOCK(s) 0
inline void SDL_LockSurface(SDL_Surface*) {}
inline void SDL_UnlockSurface(SDL_Surface*) {}
