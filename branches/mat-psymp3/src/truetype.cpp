#include "psymp3.h"

void TrueType::Init()
{
    unsigned int a = TTF_WasInit();
    if(!a && TTF_Init() == -1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        exit(2);
    }
    if(!a)
        atexit(TTF_Quit);
}
