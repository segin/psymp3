#include "psymp3.h"

Libmpg123::Libmpg123(TagLib::String name)
{
    mpg123_init();
    m_handle = mpg123_new(NULL, NULL);
}

Libmpg123::~Libmpg123()
{
    mpg123_exit();
}

void Libmpg123::open(TagLib::String name)
{

}
