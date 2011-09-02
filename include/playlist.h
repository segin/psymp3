#ifndef PLAYLIST_H
#define PLAYLIST_H

class Playlist
{
    public:
        Playlist(std::vector<std::string> args);
        Playlist(TagLib::String playlist);
        ~Playlist();
        bool addFile(TagLib::String path);
    protected:
    private:
    std::vector<track> tracks;
};

#endif // PLAYLIST_H
