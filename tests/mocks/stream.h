#ifndef STREAM_H
#define STREAM_H

#include "taglib/tstring.h"

class Stream
{
    public:
        Stream() {}
        Stream(TagLib::String name) : m_path(name) {}
        virtual ~Stream() {}
        virtual void open(TagLib::String name) {}

    protected:
        TagLib::String  m_path;
        long            m_rate = 0;
        int             m_channels = 0;
        int             m_encoding = 0;
        bool            m_eof = false;
        long long       m_position = 0; // in msec
};

#endif
