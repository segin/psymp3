#include "mutex.h"

Mutex::Mutex() : m_handle(SDL_CreateMutex())
{
    // Shouldn't really have anything to do here...
}

Mutex::~Mutex()
{
    //dtor
}
