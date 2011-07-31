#ifndef AUDIO_H
#define AUDIO_H


class Audio
{
    public:
        Audio(Stream *stream);
        virtual ~Audio();
        static void callback(void *data, Uint8 *buf, int len);
    protected:
    private:
        Stream *m_stream;
};

#endif // AUDIO_H
