#ifndef AUDIO_H
#define AUDIO_H


class Audio
{
    public:
        Audio(Stream *stream);
        ~Audio();
        void play(bool go);
        static void callback(void *data, Uint8 *buf, int len);
    protected:
    private:
        void setup();
        Stream *m_stream;
};

#endif // AUDIO_H
