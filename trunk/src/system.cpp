#include "psymp3.h"

System::System()
{
    //ctor
}

/* Get the current user's home directory. This is determined via
 * environment variables. On Windows, one could alternatively use
 * GetUserProfileDirectory(), along with OpenProcessToken() and
 * GetCurrentProcess(), but that seems a roundabout way of acomplishing
 * something so trivial when there's an environment variable that
 * does the same thing
 * Note: Seems I could also do SHGetFolderPath(), except then I need
 * to check whether we're on Vista and later, or XP and earlier, and
 * SHGetKnownFolderPath() is the one to use on Vista - and it doesn't
 * exist on earlier Windows releases. Which makes using it non-trivial
 * as one must then LoadLibrary(), etc., to get the procedure entry point
 * to avoid having a static reference in the symbol table, as that would
 * break binary compatibility with anything earlier than Vista.
 */

TagLib::String System::getHome()
{
    #ifdef _WIN32
        return getenv("USERPROFILE");
    #else
        return getenv("HOME");
    #endif
}

/* This function returns the storage path. This is determined based on
 * the %AppData% environment variable on Windows, and the $HOME
 * environment variable on Unix.
 */

TagLib::String System::getStoragePath()
{
    #ifdef _WIN32
        return getenv("APPDATA") + "\PsyMP3";
    #else
        return getHome() + "/.psymp3";
    #endif
}

/* Creates the storage path specified by getStoragePath() if it does
 * not already exist.
 */

bool System::createStoragePath()
{

}
