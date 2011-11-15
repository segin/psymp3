#include "psymp3.h"

System::System()
{
    //ctor
}

TagLib::String System::getHome()
{
    #ifdef _WIN32
        return getenv("USERPROFILE");
    #else
        return getenv("HOME");
    #endif
}

TagLib::String System::getStoragePath()
{
    #ifdef _WIN32
        return getenv("APPDATA") + "\PsyMP3";
    #else
        return getHome() + "/.psymp3";
    #endif
}
