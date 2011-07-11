#ifndef PLAYLIST_H
#define PLAYLIST_H

class Playlist
{
    public:
        /** Default constructor */
        Playlist();
        bool addFile(std::wstring path);

        /** Default destructor */
        virtual ~Playlist();
    protected:
    private:
    std::vector<track> tracks;
};

#endif // PLAYLIST_H
