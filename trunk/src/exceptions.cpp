#include "psymp3.h"

InvalidMediaException::InvalidMediaException(TagLib::String why) : m_why(why)
{
    //ctor
}

InvalidMediaException::~InvalidMediaException() throw ()
{

}

const char *InvalidMediaException::what()
{
    return m_why.toCString(true);
}

BadFormatException::BadFormatException(TagLib::String why) : m_why(why)
{
    //ctor
}

BadFormatException::~BadFormatException() throw ()
{

}

const char *BadFormatException::what()
{
    return m_why.toCString(true);
}

WrongFormatException::WrongFormatException(TagLib::String why) : m_why(why)
{
    //ctor
}

WrongFormatException::~WrongFormatException() throw ()
{

}

const char *WrongFormatException::what()
{
    return m_why.toCString(true);
}



