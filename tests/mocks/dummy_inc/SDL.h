#ifndef TEST_MOCK_SDL_H
#define TEST_MOCK_SDL_H

#include <stdint.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t Sint16;
typedef int32_t Sint32;

#define SDL_SWSURFACE 0
#define SDL_SRCALPHA 0
#define SDL_BIG_ENDIAN 4321
#define SDL_LITTLE_ENDIAN 1234
#define SDL_BYTEORDER SDL_LITTLE_ENDIAN

#define SDL_INIT_VIDEO 1

typedef struct SDL_PixelFormat {
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint32 Rmask, Gmask, Bmask, Amask;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;
} SDL_Surface;

typedef struct SDL_Rect {
    Sint16 x, y;
    Uint16 w, h;
} SDL_Rect;

#ifdef __cplusplus
extern "C" {
#endif

int SDL_Init(Uint32 flags);
void SDL_Quit(void);
char *SDL_GetError(void);
SDL_Surface *SDL_LoadBMP(const char *file);
void SDL_FreeSurface(SDL_Surface *surface);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
int SDL_SetAlpha(SDL_Surface *surface, Uint32 flags, Uint8 alpha);
int SDL_UpperBlit(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
#define SDL_BlitSurface SDL_UpperBlit
Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_FillRect(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
int SDL_Flip(SDL_Surface *screen);
int SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);

#define SDL_MUSTLOCK(S) 0

#ifdef __cplusplus
}
#endif

#endif
