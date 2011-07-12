#include "psymp3.h"

TrueType::TrueType()
{
    //ctor
}

TrueType::~TrueType()
{
    //dtor
}

void TrueType::Init()
{
    if(TTF_Init()==-1) {
        printf("TTF_Init: %s\n", TTF_GetError());
        exit(2);
    }
}
