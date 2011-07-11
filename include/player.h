#ifndef PLAYER_H
#define PLAYER_H

/* PsyMP3 main class! */

class Player
{
    public:
        Player();
        virtual ~Player();
        void Run(std::vector<std::string> args);
    protected:
    private:
        Display *display;
        Playlist *playlist;
};

#endif // PLAYER_H
