#ifndef AUDIO_H
#define AUDIO_H


class Audio
{
    public:
        Audio(Stream *stream);
        ~Audio();
        void play(bool go);
        static void callback(void *data, Uint8 *buf, int len);
        bool isPlaying() { return m_playing; };
    protected:
    private:
        void setup();
        Stream *m_stream;
        bool m_playing;
};

#endif // AUDIO_H
