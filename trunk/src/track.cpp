
#include "psymp3.h"

using namespace std;

track::track()
{
    //ctor
}

track::track(string a_Artist, string a_Title, string a_Album, string a_FilePath, unsigned int a_Len)
{
    //ctor
}

track::~track()
{
    //dtor
}

track& track::operator=(const track& rhs)
{
    if (this == &rhs) return *this; // handle self assignment
    //assignment operator
    return *this;
}
