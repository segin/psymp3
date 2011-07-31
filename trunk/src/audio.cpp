#include "psymp3.h"

Audio::Audio(Stream *stream)
{
    m_stream = stream;
    std::cout << "Audio::Audio(): " << stream->getRate() << ":" << stream->getChannels();
    setup();
}

Audio::~Audio()
{
    //dtor
}

void Audio::setup()
{
    SDL_AudioSpec fmt;
    /* Set 16-bit stereo audio at 22Khz */
    fmt.freq = m_stream->getRate();
    fmt.format = AUDIO_S16;
    fmt.channels = m_stream->getChannels();
    fmt.samples = 512;        /* A good value for games */
    fmt.callback = callback;
    fmt.userdata = (void *) m_stream;
    if ( SDL_OpenAudio(&fmt, NULL) < 0 ) {
        std::cerr << "Unable to open audio: " << SDL_GetError() << std::endl;
        // throw;
    }
}

void Audio::play(bool go)
{
    std::cout << go << std::endl;
    if (go)
        SDL_PauseAudio(0);
    else
        SDL_PauseAudio(1);
}

void Audio::callback(void *data, Uint8 *buf, int len)
{
    Stream *stream = (Stream *) data;
    stream->getData(len, (void *) buf);
}
