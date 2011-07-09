#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "track.h"

class playlist
{
    public:
        /** Default constructor */
        playlist();
        /** Default destructor */
        virtual ~playlist();
    protected:
    private:
        std::vector<track> tracks; //!< Member variable "tracks"
};

#endif // PLAYLIST_H
