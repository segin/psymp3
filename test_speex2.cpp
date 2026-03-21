#include "psymp3.h"
#ifdef HAVE_SPEEX
#include <speex/speex.h>
#include <speex/speex_header.h>
#include <speex/speex_stereo.h>
#endif
int main() {
#ifdef HAVE_SPEEX
    void* ptr = nullptr;
    speex_stereo_state_destroy((SpeexStereoState*)ptr);
#endif
    return 0;
}
