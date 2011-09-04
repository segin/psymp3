#include "psymp3.h"

Stream *MediaFile::open(TagLib::String name)
{
    TagLib::String a = name.substr(name.size() - 3).upper();
#ifdef DEBUG
    std::cout << "MediaFile::open(): " << a << std::endl;
#endif
    if(a == "MP3")
        return new Libmpg123(name);
    if(a == "OGG")
        return new Vorbis(name);
    throw InvalidMediaException("Unsupported format!");
}

