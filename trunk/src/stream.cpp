#include "psymp3.h"

Stream::Stream()
{

}

Stream::Stream(TagLib::String name)
{
    m_tags = new TagLib::FileRef(name.toCString(true));
    m_path = name;
}

Stream::~Stream()
{
    delete m_tags;
}

void Stream::open(TagLib::String name)
{
    // Base class - do nothing.
    return;
}

TagLib::String Stream::getArtist()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->artist();
}

TagLib::String Stream::getTitle()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->title();
}

TagLib::String Stream::getAlbum()
{
    if(!m_tags) return TagLib::String::null;
    return m_tags->tag()->album();
}

