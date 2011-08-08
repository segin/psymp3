#ifndef AUDIO_H
#define AUDIO_H


class Audio
{
    public:
        Audio(struct atdata *data);
        ~Audio();
        void play(bool go);
        static void callback(void *data, Uint8 *buf, int len);
        static void toFloat(int channels, int16_t *in, float *out); // 512 samples of stereo
        bool isPlaying() { return m_playing; };
    protected:
    private:
        void setup(struct atdata *data);
        Stream *m_stream;
        bool m_playing;
};

#endif // AUDIO_H
