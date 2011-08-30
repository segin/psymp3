#include "psymp3.h"

Mutex::Mutex() : m_handle(SDL_CreateMutex())
{
    // Shouldn't really have anything to do here...
}

Mutex::~Mutex()
{
    SDL_DestroyMutex(m_handle);
}

void Mutex::lock()
{
    SDL_mutexP(m_handle);
}

void Mutex::unlock()
{
    SDL_mutexV(m_handle);
}
